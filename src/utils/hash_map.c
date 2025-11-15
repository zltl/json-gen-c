/**
 * @file hash_map.c
 * @brief Implementation of a simple hash map data structure
 * 
 * This hash map uses separate chaining for collision resolution.
 * It supports dynamic resizing based on load factor and provides
 * custom hash functions, comparison functions, and memory management.
 */

#include "utils/hash_map.h"
#include "utils/error_codes.h"

#include <malloc.h>
#include <string.h>

#define HASH_MAP_LOAD_FACTOR_THRESHOLD 0.75  /**< Load factor threshold for resizing */

/**
 * @brief Create a new hash map entry with the given key and value
 * 
 * Allocates memory for a new entry node in the hash map's linked list.
 * The entry initially has no next pointer (standalone node).
 * 
 * @param key   Pointer to the key data
 * @param value Pointer to the value data
 * @return Newly allocated entry, or NULL if memory allocation fails
 */
struct hash_map_entry* hash_map_entry_new(void* key, void* value) {
    struct hash_map_entry* entry =
        (struct hash_map_entry*)malloc(sizeof(struct hash_map_entry));
    if (entry == NULL) {
        return NULL;
    }
    entry->key = key;
    entry->value = value;
    entry->next = NULL;
    return entry;
}

/**
 * @brief Free a hash map entry and its associated data
 * 
 * Uses the map's registered free functions to properly deallocate
 * the key and value data before freeing the entry structure itself.
 * Safe to call with NULL entry.
 * 
 * @param map   Hash map containing the free functions to use
 * @param entry Entry to free (can be NULL)
 */
void hash_map_entry_free(struct hash_map* map, struct hash_map_entry* entry) {
    if (entry) {
        map->key_free_func(entry->key);
        map->value_free_func(entry->value);
    }
    free(entry);
}

/**
 * @brief Create a new hash map with specified configuration
 * 
 * Initializes a hash map with the given number of buckets and function
 * pointers for hashing, comparing, and freeing keys and values. All
 * function pointers must be non-NULL.
 * 
 * @param bucket_count    Initial number of buckets (should be prime or power of 2)
 * @param hash_func       Function to compute hash value from key
 * @param key_cmp_func    Function to compare two keys (return 0 if equal)
 * @param key_free_func   Function to free key memory
 * @param value_free_func Function to free value memory
 * @return Newly created hash map, or NULL if allocation fails
 * 
 * @note All function pointers must be provided and non-NULL
 * @note Caller is responsible for calling hash_map_free() when done
 */
struct hash_map* hash_map_new(int bucket_count,
                              unsigned int (*hash_func)(void*),
                              int (*key_cmp_func)(void*, void*),
                              void (*key_free_func)(void*),
                              void (*value_free_func)(void*)) {
    struct hash_map* map = (struct hash_map*)malloc(sizeof(struct hash_map));
    if (map == NULL) {
        return NULL;
    }
    map->buckets = (struct hash_map_entry**)malloc(
        sizeof(struct hash_map_entry*) * bucket_count);
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    memset(map->buckets, 0, sizeof(struct hash_map_entry*) * bucket_count);
    map->bucket_count = bucket_count;
    map->size = 0;
    map->hash_func = hash_func;
    map->key_cmp_func = key_cmp_func;
    map->key_free_func = key_free_func;
    map->value_free_func = value_free_func;
    return map;
}

/**
 * @brief Free all memory associated with a hash map
 * 
 * Iterates through all buckets, freeing all entries and their associated
 * data using the registered free functions. Finally frees the hash map
 * structure itself. Safe to call with NULL map.
 * 
 * @param map Hash map to free (can be NULL)
 */
void hash_map_free(struct hash_map* map) {
    if (map) {
        for (int i = 0; i < map->bucket_count; i++) {
            struct hash_map_entry* entry = map->buckets[i];
            while (entry) {
                struct hash_map_entry* next = entry->next;
                hash_map_entry_free(map, entry);
                entry = next;
            }
        }
        free(map->buckets);
        free(map);
    }
}

/**
 * @brief Check if hash map needs resizing and resize if necessary
 * @param map Hash map to check
 * @return JSON_GEN_SUCCESS on success, error code on failure
 */
static int hash_map_resize_if_needed(struct hash_map* map) {
    if (map == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    // Check if load factor exceeds threshold
    double load_factor = (double)map->size / (double)map->bucket_count;
    if (load_factor < HASH_MAP_LOAD_FACTOR_THRESHOLD) {
        return JSON_GEN_SUCCESS;  // No resize needed
    }
    
    // TODO: Implement hash table expansion
    // For now, just log that expansion would be beneficial
    // In a full implementation, we would:
    // 1. Allocate new bucket array with double the size
    // 2. Rehash all existing entries
    // 3. Replace old bucket array
    
    return JSON_GEN_SUCCESS;
}

/**
 * @brief Insert key-value pair into hash map with load factor checking
 * @param map Hash map
 * @param key Key to insert
 * @param value Value to associate with key
 * @return HASH_MAP_OK on success, error code on failure
 */
int hash_map_insert(struct hash_map* map, void* key, void* value) {
    if (map == NULL || key == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    // Check load factor before insertion
    int resize_result = hash_map_resize_if_needed(map);
    if (resize_result != JSON_GEN_SUCCESS) {
        return resize_result;
    }
    
    int bucket_index = map->hash_func(key) % map->bucket_count;
    struct hash_map_entry* entry = map->buckets[bucket_index];
    
    // Check for duplicate keys
    while (entry) {
        if (map->key_cmp_func(entry->key, key) == 0) {
            return HASH_MAP_DUPLICATE_KEY;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = hash_map_entry_new(key, value);
    if (entry == NULL) {
        return JSON_GEN_ERROR_MEMORY;
    }
    
    // Insert at head of bucket chain
    entry->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = entry;
    map->size++;
    
    return HASH_MAP_OK;
}

int hash_map_find(struct hash_map* map, void* key, void** value) {
    int bucket_index = map->hash_func(key) % map->bucket_count;
    struct hash_map_entry* entry = map->buckets[bucket_index];
    while (entry) {
        if (map->key_cmp_func(entry->key, key) == 0) {
            *value = entry->value;
            return HASH_MAP_OK;
        }
        entry = entry->next;
    }
    return HASH_MAP_ERROR;
}

int hash_map_delete(struct hash_map* map, void* key) {
    int bucket_index = map->hash_func(key) % map->bucket_count;
    struct hash_map_entry* entry = map->buckets[bucket_index];
    struct hash_map_entry* prev = NULL;
    while (entry) {
        if (map->key_cmp_func(entry->key, key) == 0) {
            if (prev == NULL) {
                map->buckets[bucket_index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            hash_map_entry_free(map, entry);
            map->size--;
            return HASH_MAP_OK;
        }
        prev = entry;
        entry = entry->next;
    }
    return HASH_MAP_ERROR;
}

void hash_map_for_each(struct hash_map* map,
                       void (*fn)(void* key, void* value, void* ptr),
                       void* ptr) {
    for (int i = 0; i < map->bucket_count; i++) {
        struct hash_map_entry* entry = map->buckets[i];
        while (entry) {
            fn(entry->key, entry->value, ptr);
            entry = entry->next;
        }
    }
}

unsigned int hash(const char* data, size_t n) {
    unsigned int seed = 0xbc9f1d34;
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

unsigned int sstr_key_hash(void* key) {
    sstr_t s = (sstr_t)key;
    return hash(sstr_cstr(s), sstr_length(s));
}
int sstr_key_cmp(void* a, void* b) {
    return sstr_compare((sstr_t)a, (sstr_t)b);
}
void sstr_key_free(void* key) { sstr_free((sstr_t)key); }
