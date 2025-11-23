#ifndef UTILS_HASH_H
#define UTILS_HASH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MurmurHash2-like hash function
 * 
 * @param data Pointer to the data to hash
 * @param n Length of the data
 * @param seed Initial seed value
 * @return unsigned int The hash value
 */
unsigned int hash_murmur(const char* data, size_t n, unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif // UTILS_HASH_H
