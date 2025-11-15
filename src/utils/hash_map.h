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
 * @brief Return codes for hash_map operations
 * 
 * Consistent error codes used across all hash map functions to indicate
 * success or various failure conditions.
 */

/** Operation completed successfully */
#define HASH_MAP_OK 0
/** Attempted to insert a key that already exists */
#define HASH_MAP_DUPLICATE_KEY 1
/** Memory allocation failed or key not found */
#define HASH_MAP_ERROR -1

/**
 * @brief Entry node in hash map's linked list (for collision handling)
 * 
 * Each bucket in the hash map contains a linked list of entries.
 * When hash collisions occur, new entries are added to the list.
 */
struct hash_map_entry {
    void *key;                      /**< Pointer to key data */
    void *value;                    /**< Pointer to value data */
    struct hash_map_entry *next;    /**< Next entry in collision chain */
};

/**
 * @brief Hash map data structure using separate chaining
 * 
 * A hash table that maps keys to values using separate chaining for
 * collision resolution. Supports custom hash functions, comparison
 * functions, and memory management callbacks.
 */
struct hash_map {
    struct hash_map_entry **buckets;           /**< Array of bucket heads */
    int bucket_count;                          /**< Number of buckets */
    int size;                                  /**< Total number of entries */
    unsigned int (*hash_func)(void *);         /**< Custom hash function */
    int (*key_cmp_func)(void *, void *);      /**< Key comparison (0 if equal) */
    void (*key_free_func)(void *);            /**< Key memory cleanup */
    void (*value_free_func)(void *);          /**< Value memory cleanup */
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
 * @brief Insert a new key-value pair into the hash map
 * 
 * Adds a new entry to the hash map. If the key already exists, returns
 * HASH_MAP_DUPLICATE_KEY and does not modify the map. Uses the registered
 * hash function to determine the bucket and adds to the collision chain.
 * Automatically checks load factor and may trigger resizing.
 *
 * @param map Hash map to insert into
 * @param key Key pointer (ownership transferred to map if successful)
 * @param value Value pointer (ownership transferred to map if successful)
 * @return HASH_MAP_OK on success
 * @return HASH_MAP_DUPLICATE_KEY if key already exists (entry not added)
 * @return HASH_MAP_ERROR if memory allocation fails
 * 
 * @note If insertion fails, caller retains ownership of key and value
 * @note If successful, map owns key and value (freed with registered functions)
 */
extern int hash_map_insert(struct hash_map *map, void *key, void *value);

/**
 * @brief Find a value by key in the hash map
 * 
 * Searches for the given key and returns the associated value if found.
 * Uses the registered hash and comparison functions for lookup.
 *
 * @param map Hash map to search
 * @param key Key to search for
 * @param value Output pointer to store found value (if found)
 * @return HASH_MAP_OK if key found (value pointer stored in *value)
 * @return HASH_MAP_ERROR if key not found (value pointer undefined)
 * 
 * @note The returned value pointer is still owned by the map
 * @note Do not free the returned value pointer
 */
extern int hash_map_find(struct hash_map *map, void *key, void **value);

/**
 * @brief Remove an entry from the hash map by key
 * 
 * Searches for the entry with the given key and removes it from the map.
 * The entry's key and value are freed using the registered free functions
 * before removing.
 *
 * @param map Hash map to modify
 * @param key Key of entry to remove
 * @return HASH_MAP_OK if entry found and removed
 * @return HASH_MAP_ERROR if key not found
 * 
 * @note The entry's key and value are automatically freed
 * @note After deletion, any pointers to the entry become invalid
 */
extern int hash_map_delete(struct hash_map *map, void *key);

/**
 * @brief Iterate over all entries in the hash map
 * 
 * Calls the provided callback function for each key-value pair in the
 * hash map. The iteration order is undefined (based on bucket order).
 * Useful for operations like printing, serialization, or cleanup.
 *
 * @param map Hash map to iterate over
 * @param fn Callback function called for each entry
 * @param ptr User data pointer passed to each callback invocation
 * 
 * @note Do not modify the hash map structure during iteration
 * @note The callback receives (key, value, ptr) for each entry
 */
extern void hash_map_for_each(struct hash_map *map,
                       void (*fn)(void *key, void *value, void *ptr),
                       void *ptr);

/**
 * @brief Compute hash value for raw byte data
 * 
 * General-purpose hash function for arbitrary byte sequences.
 * Uses a good distribution algorithm suitable for hash tables.
 *
 * @param data Pointer to data buffer to hash
 * @param n Length of data in bytes
 * @return 32-bit hash value with good distribution
 */
extern unsigned int hash(const char *data, size_t n);

/**
 * @brief Hash function for sstr_t keys
 * 
 * Specialized hash function for use with hash maps that have sstr_t keys.
 * Pass this to hash_map_new() as the hash_func parameter.
 *
 * @param key Pointer to sstr_t to hash
 * @return Hash value for the string
 */
extern unsigned int sstr_key_hash(void *key);

/**
 * @brief Comparison function for sstr_t keys
 * 
 * Specialized comparison for use with hash maps that have sstr_t keys.
 * Pass this to hash_map_new() as the key_cmp_func parameter.
 *
 * @param a First sstr_t key
 * @param b Second sstr_t key
 * @return 0 if equal, non-zero if different
 */
extern int sstr_key_cmp(void *a, void *b);

/**
 * @brief Free function for sstr_t keys
 * 
 * Specialized free function for use with hash maps that have sstr_t keys.
 * Pass this to hash_map_new() as the key_free_func parameter.
 *
 * @param key Pointer to sstr_t to free
 */
extern void sstr_key_free(void *key);

#ifdef __cplusplus
}
#endif

#endif  // HASH_MAP_H
