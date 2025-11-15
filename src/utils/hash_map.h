/**
 * @file utils/hash_map.h
 * @brief A simple hash_map implementation.
 */

#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stddef.h>

#include "utils/sstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief return code of hash_map functions.
 */

// operation success
#define HASH_MAP_OK 0
// duplicate key
#define HASH_MAP_DUPLICATE_KEY 1
// malloc failed
#define HASH_MAP_ERROR -1

struct hash_map_entry {
    void *key;
    void *value;
    struct hash_map_entry *next;
};

struct hash_map {
    struct hash_map_entry **buckets;
    int bucket_count;
    int size;
    unsigned int (*hash_func)(void *);
    int (*key_cmp_func)(void *, void *);
    void (*key_free_func)(void *);
    void (*value_free_func)(void *);
};

/**
 * @brief create a new hash_map_entry with key and value.
 *
 * @param key the key of the entry.
 * @param value the value of the entry.
 * @return struct hash_map_entry* the new entry.
 * @retval NULL malloc failed.
 * @retval !NULL the new entry.
 */
extern struct hash_map_entry *hash_map_entry_new(void *key, void *value);

/**
 * @brief free a hash_map_entry. the key and value will be freed by
 *       key_free_func and value_free_func of \a map.
 *
 * @param map  the hash_map, container map->key_free_func and
 * map->value_free_func.
 * @param entry the entry to be freed.
 */
extern void hash_map_entry_free(struct hash_map *map, struct hash_map_entry *entry);

/**
 * @brief create a new hash_map.
 *
 * @param bucket_count the number of buckets.
 * @param hash_func the hash function.
 * @param key_cmp_func the key compare function, return 0 if equal, else not 0.
 * @param key_free_func the key free function.
 * @param value_free_func the value free function.
 * @return struct hash_map* the new hash_map.
 */
extern struct hash_map *hash_map_new(int bucket_count,
                              unsigned int (*hash_func)(void *),
                              int (*key_cmp_func)(void *, void *),
                              void (*key_free_func)(void *),
                              void (*value_free_func)(void *));

/**
 * @brief free a hash_map.
 *
 * @param map the hash_map to be freed.
 */
extern void hash_map_free(struct hash_map *map);

/**
 * @brief insert a new entry into hash_map.
 *
 * @param map the hash_map.
 * @param key the key of the entry.
 * @param value the value of the entry.
 * @return int HASH_MAP_OK if success, HASH_MAP_DUPLICATE_KEY will ignore the
 *        entry, HASH_MAP_ERROR if malloc failed.
 */
extern int hash_map_insert(struct hash_map *map, void *key, void *value);

/**
 * @brief find a entry in hash_map.
 *
 * @param map the hash_map.
 * @param key the key of the entry.
 * @param value the value of the entry if found.
 * @return int HASH_MAP_OK if found, HASH_MAP_ERROR if not found.
 */
extern int hash_map_find(struct hash_map *map, void *key, void **value);

/**
 * @brief remove a entry from hash_map.
 *
 * @param map the hash_map.
 * @param key the key of the entry.
 * @return int HASH_MAP_OK if found, HASH_MAP_ERROR if not found.
 * @note the entry will be free.
 */
extern int hash_map_delete(struct hash_map *map, void *key);

/**
 * @brief for each key-value pair in hash_map, call fn(key, value).
 *
 * @param map the hash_map.
 * @param fn the function to be called.
 * @param ptr user data.
 */
extern void hash_map_for_each(struct hash_map *map,
                       void (*fn)(void *key, void *value, void *ptr),
                       void *ptr);

/**
 * @brief calculate the hash value of a key.
 *
 * @param data pointer to key buffer.
 * @param n the length of key buffer.
 * @return unsigned int the hash value.
 */
extern unsigned int hash(const char *data, size_t n);

extern unsigned int sstr_key_hash(void *key);
extern int sstr_key_cmp(void *a, void *b);
extern void sstr_key_free(void *key);

#ifdef __cplusplus
}
#endif

#endif  // HASH_MAP_H
