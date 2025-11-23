#include "utils/hash.h"

unsigned int hash_murmur(const char* data, size_t n, unsigned int seed) {
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
