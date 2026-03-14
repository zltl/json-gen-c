/*
 * Fuzz harness for the JSON unmarshal path of json-gen-c generated code.
 *
 * Build with clang -fsanitize=fuzzer,address and link against the
 * generated json.gen.o + sstr.o from fuzz/.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "json.gen.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 64 * 1024)
        return 0;

    sstr_t input = sstr_of((const char *)data, (int)size);
    if (input == NULL)
        return 0;

    /* Exercise the unmarshal path — invalid JSON is expected. */
    struct FuzzTarget obj;
    FuzzTarget_init(&obj);
    (void)json_unmarshal_FuzzTarget(input, &obj);
    FuzzTarget_clear(&obj);

    sstr_free(input);
    return 0;
}
