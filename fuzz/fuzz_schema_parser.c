/*
 * Fuzz harness for the json-gen-c schema parser.
 *
 * Build with clang -fsanitize=fuzzer,address and link against
 * libstruct + libutils from the main build.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "struct/struct_parse.h"
#include "utils/sstr.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Limit input size to avoid timeouts on extremely large inputs. */
    if (size > 64 * 1024)
        return 0;

    /* Create a NUL-terminated sstr from the fuzzer input. */
    sstr_t input = sstr_of((const char *)data, (int)size);
    if (input == NULL)
        return 0;

    struct struct_parser *parser = struct_parser_new();
    if (parser == NULL) {
        sstr_free(input);
        return 0;
    }

    /* Parse — errors are expected and handled internally. */
    (void)struct_parser_parse(parser, input);

    struct_parser_free(parser);
    sstr_free(input);
    return 0;
}
