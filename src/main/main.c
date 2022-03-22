#include <stdio.h>
#include <string.h>

#include "gencode/gencode.h"
#include "struct/struct_parse.h"
#include "utils/io.h"

int main() {
    struct struct_parser* parser = struct_parser_new();
    if (parser == NULL) {
        return -1;
    }

    sstr_t content = sstr_new();
    int r = read_file("example.json-gen-c", content);
    if (r != 0) {
        fprintf(stderr, "error reading file");
        sstr_free(content);
        return -1;
    }

    struct_parser_parse(parser, content);

    sstr_t source = sstr_new();
    sstr_t head = sstr_new();
    gencode_source(parser->struct_map, source, head);

    write_file("example.c", source);
    write_file("example.h", head);

    sstr_free(source);
    sstr_free(head);

    struct_parser_free(parser);
    sstr_free(content);

    return 0;
}
