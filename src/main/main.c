#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gencode/gencode.h"
#include "struct/struct_parse.h"
#include "utils/io.h"
#include "utils/error_codes.h"

/**
 * @brief Display usage information
 * @param stream Output stream (stdout or stderr)
 */
static void usage(FILE *stream) {
    fprintf(stream,
        "Usage: json-gen-c -out <output_dir> -in <input_file>\n"
        "Generate JSON operating C codes from struct definition.\n\n"
        "Options:\n"
        "    -in <input_file>  Specify the input struct definition file.\n"
        "    -out <output_dir> Specify the output codes location, default to current directory\n"
        "    -h, --help        Show this help message\n\n"
        "json-gen-c document: https://github.com/zltl/json-gen-c\n"
        "Report bugs to: https://github.com/zltl/json-gen-c/issues\n");
}

/**
 * @brief Command line options structure
 */
struct options {
    char *input_file;   /**< Input struct definition file path */
    char *output_path;  /**< Output directory path */
};

/**
 * @brief Parse command line options
 * @param argc Argument count
 * @param argv Argument vector
 * @param options Output options structure
 * @return JSON_GEN_SUCCESS on success, error code on failure
 */
static json_gen_error_t options_parse(int argc, const char **argv, struct options *options) {
    if (options == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-in") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -in option requires an argument\n");
                return JSON_GEN_ERROR_INVALID_PARAM;
            }
            options->input_file = (char *)argv[++i];
        } else if (strcmp(argv[i], "-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -out option requires an argument\n");
                return JSON_GEN_ERROR_INVALID_PARAM;
            }
            options->output_path = (char *)argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            exit(JSON_GEN_SUCCESS);
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
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

int main(int argc, const char **argv) {
    // Initialize options structure
    struct options options = {NULL, NULL};
    
    // Parse command line options
    json_gen_error_t result = options_parse(argc, argv, &options);
    if (result != JSON_GEN_SUCCESS) {
        usage(stderr);
        return result;
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

    // Parse the struct definition
    r = struct_parser_parse(parser, content);
    if (r < 0) {
        fprintf(stderr, "Error: struct parsing failed\n");
        cleanup_and_exit(content, parser, NULL, NULL, JSON_GEN_ERROR_PARSE);
    }

    // Generate the output json.gen.c and json.gen.h files
    sstr_t source = sstr_new();
    sstr_t head = sstr_new();
    if (source == NULL || head == NULL) {
        fprintf(stderr, "Error: failed to allocate memory for output generation\n");
        cleanup_and_exit(content, parser, source, head, JSON_GEN_ERROR_MEMORY);
    }
    
    r = gencode_source(parser->struct_map, source, head);
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
    sstr_append_of(out_c_file, OUTPUT_C_FILENAME, strlen(OUTPUT_C_FILENAME));
    sstr_append_of(out_h_file, "/", 1);
    sstr_append_of(out_h_file, OUTPUT_H_FILENAME, strlen(OUTPUT_H_FILENAME));
    
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

    // Clean up main resources
    sstr_free(source);
    sstr_free(head);
    struct_parser_free(parser);
    sstr_free(content);

    printf("Code generation completed successfully\n");
    return JSON_GEN_SUCCESS;
}
