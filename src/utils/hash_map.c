#include "utils/hash_map.h"
#include "utils/error_codes.h"

#include <malloc.h>
#include <string.h>

#define HASH_MAP_LOAD_FACTOR_THRESHOLD 0.75  /**< Load factor threshold for resizing */

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

void hash_map_entry_free(struct hash_map* map, struct hash_map_entry* entry) {
    if (entry) {
        map->key_free_func(entry->key);
        map->value_free_func(entry->value);
    }
    free(entry);
}

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
    
    int new_bucket_count = map->bucket_count * 2;
    struct hash_map_entry** new_buckets = (struct hash_map_entry**)malloc(
        sizeof(struct hash_map_entry*) * new_bucket_count);
    if (new_buckets == NULL) {
        return JSON_GEN_ERROR_MEMORY;
    }
    memset(new_buckets, 0, sizeof(struct hash_map_entry*) * new_bucket_count);

    // Rehash all existing entries
    for (int i = 0; i < map->bucket_count; i++) {
        struct hash_map_entry* entry = map->buckets[i];
        while (entry) {
            struct hash_map_entry* next = entry->next;
            
            // Calculate new index
            int new_index = map->hash_func(entry->key) % new_bucket_count;
            
            // Insert at head of new bucket
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            
            entry = next;
        }
    }

    // Free old buckets array
    free(map->buckets);
    
    // Update map
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    
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
