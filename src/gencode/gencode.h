/**
 * @file gencode.h
 * @author your name (you@domain.com)
 * @brief generate json manipulate code.
 */

#ifndef GENCODE_H_
#define GENCODE_H_

#include "utils/hash_map.h"
#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OUTPUT_C_FILENAME "json.gen.c"
#define OUTPUT_H_FILENAME "json.gen.h"

int gencode_source(struct hash_map* struct_map, sstr_t source, sstr_t header);

static unsigned int hash_s(const char* data, size_t n, unsigned int seed) {
    // unsigned int seed = 0xbc9f1d34;
    // Similar to murmur hash
    const unsigned int m = 0xc6a4a793;
    const unsigned int r = 24;
    const char* limit = data + n;
    unsigned int h = seed ^ (n * m);

    // Pick up four bytes at a time
    while (data + 4 <= limit) {
        unsigned int w = *(unsigned int*)(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    // Pick up remaining bytes
    switch (limit - data) {
        case 3:
            h += (unsigned char)(data[2]) << 16;
            __attribute__((fallthrough));
        case 2:
            h += (unsigned char)(data[1]) << 8;
            __attribute__((fallthrough));
        case 1:
            h += (unsigned char)(data[0]);
            h *= m;
            h ^= (h >> r);
            break;
    }
    return h;
}

/*
static unsigned int hash_2(char* key1, char* key2) {
    unsigned int res = 0xbc9f1d34;
    res = hash_s(key1, strlen(key1), res);
    res = hash_s(key2, strlen(key2), res);
    return res;
}
*/

#include <stdio.h>
inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int res = 0xbc9f1d34;
    sstr_t tmp = sstr_dup(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append(tmp, key2);
    res = hash_s(sstr_cstr(tmp), sstr_length(tmp), res);
    sstr_free(tmp);
    return res;
}

#ifdef __cplusplus
}
#endif

#endif  // GENCODE_H_
