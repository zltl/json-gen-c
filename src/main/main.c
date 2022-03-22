#include <stdio.h>
#include <string.h>

#include "extra_codes_sstr.inc"
#include "gencode/gencode.h"
#include "struct/struct_parse.h"
#include "utils/io.h"

static void usage() {
    printf("Usage: json-gen-c -out <output_dir> -in <input_file>\n");
}

struct options {
    char *input_file;
    char *output_path;
};

static int options_parse(int argc, const char **argv, struct options *options) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-in") == 0) {
            options->input_file = (char *)argv[++i];
        } else if (strcmp(argv[i], "-out") == 0) {
            options->output_path = (char *)argv[++i];
        } else {
            fprintf(stderr, "unkown option %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, const char **argv) {
    struct options options = {NULL, NULL};
    options_parse(argc, argv, &options);
    if (options.input_file == NULL) {
        usage();
        return -1;
    }

    struct struct_parser *parser = struct_parser_new();
    if (parser == NULL) {
        return -1;
    }

    sstr_t content = sstr_new();
    int r = read_file(options.input_file, content);
    if (r != 0) {
        fprintf(stderr, "error reading file %s", options.input_file);
        sstr_free(content);
        return -1;
    }

    r = struct_parser_parse(parser, content);
    if (r < 0) {
        fprintf(stderr, "struct parse failed\n");
        sstr_free(content);
        return -1;
    }

    sstr_t source = sstr_new();
    sstr_t head = sstr_new();
    r = gencode_source(parser->struct_map, source, head);
    if (r < 0) {
        fprintf(stderr, "gencode source failed\n");
        sstr_free(content);
        return -1;
    }
    sstr_t out_c_file = sstr(options.output_path);
    sstr_append_of(out_c_file, "/", 1);
    sstr_append_of(out_c_file, OUTPUT_C_FILENAME, strlen(OUTPUT_C_FILENAME));
    sstr_t out_h_file = sstr(options.output_path);
    sstr_append_of(out_h_file, "/", 1);
    sstr_append_of(out_h_file, OUTPUT_H_FILENAME, strlen(OUTPUT_H_FILENAME));
    write_file(sstr_cstr(out_c_file), source);
    write_file(sstr_cstr(out_h_file), head);
    sstr_free(out_c_file);
    sstr_free(out_h_file);
    sstr_free(source);
    sstr_free(head);

    sstr_t source_ext = sstr(options.output_path);
    sstr_t head_ext = sstr(options.output_path);
    sstr_append_cstr(source_ext, "/sstr.c");
    sstr_append_cstr(head_ext, "/sstr.h");
    sstr_t source_ext_c = sstr_of(utils_sstr_c, utils_sstr_c_len);
    sstr_t head_ext_h = sstr_of(utils_sstr_h, utils_sstr_h_len);
    write_file(sstr_cstr(source_ext), source_ext_c);
    write_file(sstr_cstr(head_ext), head_ext_h);
    sstr_free(source_ext_c);
    sstr_free(head_ext_h);
    sstr_free(source_ext);
    sstr_free(head_ext);

    struct_parser_free(parser);
    sstr_free(content);

    return 0;
}
