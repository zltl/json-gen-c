#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/getopt_compat.h"

#include "compat/compat_check.h"
#include "gencode/gencode.h"
#include "lsp/lsp_server.h"
#include "struct/struct_parse.h"
#include "utils/io.h"
#include "utils/error_codes.h"

#ifndef JSON_GEN_C_VERSION
#define JSON_GEN_C_VERSION "unknown"
#endif

/** Output format for code generation. */
enum output_format {
    FORMAT_JSON = 0,
    FORMAT_MSGPACK = 1,
    FORMAT_CBOR = 2,
};

/**
 * @brief Display usage information
 * @param stream Output stream (stdout or stderr)
 */
static void usage(FILE *stream) {
    fprintf(stream,
        "Usage: json-gen-c -out <output_dir> -in <input_file> [--format json|msgpack|cbor] [--cpp-wrapper]\n"
        "       json-gen-c --check-compat <old_schema> <new_schema>\n"
        "       json-gen-c --lsp\n"
        "Generate serialization C code from struct definition.\n\n"
        "Options:\n"
        "    -in <input_file>     Specify the input struct definition file.\n"
        "    -out <output_dir>    Specify the output codes location, default to current directory\n"
        "    --format <format>    Output format: json (default), msgpack, or cbor\n"
        "    --cpp-wrapper        Also generate a C++ wrapper header (.gen.hpp)\n"
        "    --check-compat       Compare two schemas and report compatibility.\n"
        "    --lsp                Run as Language Server Protocol (LSP) server.\n"
        "    -h, --help           Show this help message\n"
        "    -v, --version        Show version information\n\n"
        "json-gen-c document: https://github.com/zltl/json-gen-c\n"
        "Report bugs to: https://github.com/zltl/json-gen-c/issues\n");
}

/**
 * @brief Command line options structure
 */
struct options {
    char *input_file;      /**< Input struct definition file path */
    char *output_path;     /**< Output directory path */
    char *compat_old;      /**< Old schema for --check-compat */
    char *compat_new;      /**< New schema for --check-compat */
    enum output_format format; /**< Output serialization format */
    int cpp_wrapper;       /**< Generate C++ wrapper header (.gen.hpp) */
    int lsp_mode;          /**< Run as LSP server */
};

/**
 * @brief Parse command line options
 * @param argc Argument count
 * @param argv Argument vector
 * @param options Output options structure
 * @return JSON_GEN_SUCCESS on success, error code on failure
 */
static json_gen_error_t options_parse(int argc, char **argv, struct options *options) {
    if (options == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }

    /* Handle --check-compat before getopt (it takes two positional args). */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check-compat") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr, "Error: --check-compat requires two arguments: "
                                "<old_schema> <new_schema>\n");
                return JSON_GEN_ERROR_INVALID_PARAM;
            }
            options->compat_old = argv[i + 1];
            options->compat_new = argv[i + 2];
            return JSON_GEN_SUCCESS;
        }
        if (strcmp(argv[i], "--lsp") == 0) {
            options->lsp_mode = 1;
            return JSON_GEN_SUCCESS;
        }
    }

    static struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"out", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"cpp-wrapper", no_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int c;
    int option_index = 0;

    // Reset getopt index
    optind = 1;

    while ((c = getopt_long_only(argc, argv, "i:o:f:chv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'i':
                options->input_file = optarg;
                break;
            case 'o':
                options->output_path = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "json") == 0) {
                    options->format = FORMAT_JSON;
                } else if (strcmp(optarg, "msgpack") == 0) {
                    options->format = FORMAT_MSGPACK;
                } else if (strcmp(optarg, "cbor") == 0) {
                    options->format = FORMAT_CBOR;
                } else {
                    fprintf(stderr, "Error: unknown format '%s' (expected json, msgpack, or cbor)\n", optarg);
                    return JSON_GEN_ERROR_INVALID_PARAM;
                }
                break;
            case 'c':
                options->cpp_wrapper = 1;
                break;
            case 'h':
                usage(stdout);
                exit(JSON_GEN_SUCCESS);
            case 'v':
                printf("json-gen-c %s\n", JSON_GEN_C_VERSION);
                exit(JSON_GEN_SUCCESS);
            case '?':
                // getopt_long_only prints error message to stderr automatically
                return JSON_GEN_ERROR_INVALID_PARAM;
            default:
                return JSON_GEN_ERROR_INVALID_PARAM;
        }
    }
    
    // Set default output path if not specified
    if (options->output_path == NULL) {
        options->output_path = "./";
    }
    
    return JSON_GEN_SUCCESS;
}

/**
 * @brief Clean up resources and exit
 * @param content Content string to free (can be NULL)
 * @param parser Parser to free (can be NULL)
 * @param source Source string to free (can be NULL)
 * @param head Header string to free (can be NULL)
 * @param exit_code Exit code to return
 */
static void cleanup_and_exit(sstr_t content, struct struct_parser *parser, 
                           sstr_t source, sstr_t head, int exit_code) {
    if (content) sstr_free(content);
    if (parser) struct_parser_free(parser);
    if (source) sstr_free(source);
    if (head) sstr_free(head);
    exit(exit_code);
}

/**
 * @brief Parse a schema file into a parser context.
 * @param path File path.
 * @param parser Output parser (must be pre-created).
 * @return 0 on success, non-zero on failure.
 */
static int parse_schema_file(const char *path, struct struct_parser *parser) {
    sstr_t content = sstr_new();
    if (content == NULL) return -1;
    if (read_file(path, content) != 0) {
        fprintf(stderr, "Error: failed to read '%s'\n", path);
        sstr_free(content);
        return -1;
    }
    parser->name = (char *)path;
    struct include_node root_inc;
    root_inc.path = path;
    root_inc.parent = NULL;
    parser->include_stack = &root_inc;
    int r = struct_parser_parse(parser, content);
    sstr_free(content);
    if (r < 0) return -1;
    return struct_parser_validate(parser);
}

/**
 * @brief Run the --check-compat workflow.
 * @return 0 if schemas are compatible, 1 if breaking changes, 2+ on error.
 */
static int run_compat_check(const char *old_path, const char *new_path) {
    struct struct_parser *old_p = struct_parser_new();
    struct struct_parser *new_p = struct_parser_new();
    if (old_p == NULL || new_p == NULL) {
        fprintf(stderr, "Error: failed to allocate parsers\n");
        if (old_p) struct_parser_free(old_p);
        if (new_p) struct_parser_free(new_p);
        return 2;
    }
    if (parse_schema_file(old_path, old_p) < 0) {
        fprintf(stderr, "Error: failed to parse old schema '%s'\n", old_path);
        struct_parser_free(old_p);
        struct_parser_free(new_p);
        return 2;
    }
    if (parse_schema_file(new_path, new_p) < 0) {
        fprintf(stderr, "Error: failed to parse new schema '%s'\n", new_path);
        struct_parser_free(old_p);
        struct_parser_free(new_p);
        return 2;
    }
    int rc = compat_check(old_p->struct_map, old_p->enum_map,
                          old_p->oneof_map, new_p->struct_map,
                          new_p->enum_map, new_p->oneof_map);
    struct_parser_free(old_p);
    struct_parser_free(new_p);
    return rc;
}

int main(int argc, char **argv) {
    // Initialize options structure
    struct options options = {NULL, NULL, NULL, NULL, FORMAT_JSON, 0, 0};
    
    // Parse command line options
    json_gen_error_t result = options_parse(argc, argv, &options);
    if (result != JSON_GEN_SUCCESS) {
        usage(stderr);
        return result;
    }

    /* --check-compat mode: compare two schemas and exit. */
    if (options.compat_old != NULL) {
        return run_compat_check(options.compat_old, options.compat_new);
    }

    /* --lsp mode: run language server and exit. */
    if (options.lsp_mode) {
        return lsp_server_run();
    }
    
    // Validate required input file parameter
    if (options.input_file == NULL) {
        fprintf(stderr, "Error: input file is required\n");
        usage(stderr);
        return JSON_GEN_ERROR_INVALID_PARAM;
    }

    // Read content of input file
    sstr_t content = sstr_new();
    if (content == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for content\n");
        return JSON_GEN_ERROR_MEMORY;
    }
    
    int r = read_file(options.input_file, content);
    if (r != 0) {
        fprintf(stderr, "Error: failed to read file '%s'\n", options.input_file);
        cleanup_and_exit(content, NULL, NULL, NULL, JSON_GEN_ERROR_FILE_IO);
    }

    // Create new parser
    struct struct_parser *parser = struct_parser_new();
    if (parser == NULL) {
        fprintf(stderr, "Error: failed to create parser\n");
        cleanup_and_exit(content, NULL, NULL, NULL, JSON_GEN_ERROR_MEMORY);
    }
    parser->name = options.input_file;
    // Set up include stack with the root file for circular detection
    struct include_node root_include;
    root_include.path = options.input_file;
    root_include.parent = NULL;
    parser->include_stack = &root_include;

    // Parse the struct definition
    r = struct_parser_parse(parser, content);
    if (r < 0) {
        // Diagnostics already printed by the parser's diag engine
        cleanup_and_exit(content, parser, NULL, NULL, JSON_GEN_ERROR_PARSE);
    }

    // Validate schema before code generation
    r = struct_parser_validate(parser);
    if (r < 0) {
        cleanup_and_exit(content, parser, NULL, NULL, JSON_GEN_ERROR_PARSE);
    }

    // Generate output code based on selected format
    sstr_t source = sstr_new();
    sstr_t head = sstr_new();
    if (source == NULL || head == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for output generation\n");
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
    }

    const char *out_c_name;
    const char *out_h_name;

    if (options.format == FORMAT_MSGPACK) {
        r = gencode_msgpack_source(parser->struct_map, parser->enum_map,
                                   parser->oneof_map, source, head);
        out_c_name = OUTPUT_MSGPACK_C_FILENAME;
        out_h_name = OUTPUT_MSGPACK_H_FILENAME;
    } else if (options.format == FORMAT_CBOR) {
        r = gencode_cbor_source(parser->struct_map, parser->enum_map,
                                parser->oneof_map, source, head);
        out_c_name = OUTPUT_CBOR_C_FILENAME;
        out_h_name = OUTPUT_CBOR_H_FILENAME;
    } else {
        r = gencode_source(parser->struct_map, parser->enum_map,
                           parser->oneof_map, source, head);
        out_c_name = OUTPUT_C_FILENAME;
        out_h_name = OUTPUT_H_FILENAME;
    }
    if (r < 0) {
        fprintf(stderr, "Error: code generation failed\n");
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_GENERAL);
    }
    
    // Write output files with proper error checking
    sstr_t out_c_file = sstr(options.output_path);
    sstr_t out_h_file = sstr(options.output_path);
    if (out_c_file == NULL || out_h_file == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for file paths\n");
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
    }
    
    sstr_append_of(out_c_file, "/", 1);
    sstr_append_of(out_c_file, out_c_name, strlen(out_c_name));
    sstr_append_of(out_h_file, "/", 1);
    sstr_append_of(out_h_file, out_h_name, strlen(out_h_name));
    
    if (write_file(sstr_cstr(out_c_file), source) != 0) {
        fprintf(stderr, "Error: failed to write output C file '%s'\n", sstr_cstr(out_c_file));
        sstr_free(out_c_file);
        sstr_free(out_h_file);
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_FILE_IO);
    }
    
    if (write_file(sstr_cstr(out_h_file), head) != 0) {
        fprintf(stderr, "Error: failed to write output header file '%s'\n", sstr_cstr(out_h_file));
        sstr_free(out_c_file);
        sstr_free(out_h_file);
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_FILE_IO);
    }
    
    sstr_free(out_c_file);
    sstr_free(out_h_file);
    // Write utility files (sstr.h, sstr.c) with error checking
    sstr_t source_ext = sstr(options.output_path);
    sstr_t head_ext = sstr(options.output_path);
    if (source_ext == NULL || head_ext == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for utility file paths\n");
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
    }
    
    sstr_append_cstr(source_ext, "/sstr.c");
    sstr_append_cstr(head_ext, "/sstr.h");
    
    // Include utility source code
#include "extra_codes_sstr.inc"
    sstr_t source_ext_c = sstr_of(sstr_c, sstr_c_len);
    sstr_t head_ext_h = sstr_of(sstr_h, sstr_h_len);
    
    if (source_ext_c == NULL || head_ext_h == NULL) {
        fprintf(stderr, "Error: failed to create utility file content\n");
        sstr_free(source_ext);
        sstr_free(head_ext);
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
    }
    
    if (write_file(sstr_cstr(source_ext), source_ext_c) != 0) {
        fprintf(stderr, "Error: failed to write sstr.c file\n");
        sstr_free(source_ext_c);
        sstr_free(head_ext_h);
        sstr_free(source_ext);
        sstr_free(head_ext);
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_FILE_IO);
    }
    
    if (write_file(sstr_cstr(head_ext), head_ext_h) != 0) {
        fprintf(stderr, "Error: failed to write sstr.h file\n");
        sstr_free(source_ext_c);
        sstr_free(head_ext_h);
        sstr_free(source_ext);
        sstr_free(head_ext);
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_FILE_IO);
    }
    
    // Clean up utility file resources
    sstr_free(source_ext_c);
    sstr_free(head_ext_h);
    sstr_free(source_ext);
    sstr_free(head_ext);

    // Generate C++ wrapper header if requested
    if (options.cpp_wrapper) {
        sstr_t cpp_header = sstr_new();
        if (cpp_header == NULL) {
            fprintf(stderr, "Error: failed to allocate memory for C++ wrapper\n");
            cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
        }
        r = gencode_cpp_wrapper(parser->struct_map, parser->enum_map,
                                parser->oneof_map, out_h_name,
                                options.format, cpp_header);
        if (r < 0) {
            fprintf(stderr, "Error: C++ wrapper generation failed\n");
            sstr_free(cpp_header);
            cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_GENERAL);
        }
        sstr_t cpp_path = sstr(options.output_path);
        sstr_append_of(cpp_path, "/", 1);
        sstr_append_cstr(cpp_path, OUTPUT_CPP_FILENAME);
        if (write_file(sstr_cstr(cpp_path), cpp_header) != 0) {
            fprintf(stderr, "Error: failed to write C++ wrapper '%s'\n",
                    sstr_cstr(cpp_path));
            sstr_free(cpp_path);
            sstr_free(cpp_header);
            cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_FILE_IO);
        }
        sstr_free(cpp_path);
        sstr_free(cpp_header);
    }

    // Clean up main resources
    sstr_free(source);
    sstr_free(head);
    struct_parser_free(parser);
    sstr_free(content);

    printf("Code generation completed successfully\n");
    return JSON_GEN_SUCCESS;
}
