/**
 * @file json_context.h
 * @brief Thread-safe context structure for JSON parsing
 */

#ifndef JSON_CONTEXT_H
#define JSON_CONTEXT_H

#include <stdint.h>
#include <pthread.h>

/**
 * @brief Field offset item structure for JSON parsing
 */
struct json_field_offset_item {
    int offset;
    int type_size;
    int field_type;
    const char* field_type_name;
    const char* field_name;
    const char* struct_name;
    int is_array;
};

/**
 * @brief Thread-safe JSON parsing context
 * This structure replaces global variables to ensure thread safety
 */
struct json_context {
    struct json_field_offset_item *field_offset_items;  /**< Field offset items array */
    int *entry_hash;                                    /**< Hash table for field lookups */
    int entry_hash_size;                               /**< Size of hash table */
    int item_count;                                    /**< Number of field offset items */
    pthread_mutex_t mutex;                             /**< Mutex for thread safety */
};

/**
 * @brief Create a new JSON parsing context
 * @return Pointer to new context, or NULL on failure
 */
extern struct json_context* json_context_new(void);

/**
 * @brief Free a JSON parsing context
 * @param ctx Context to free
 */
extern void json_context_free(struct json_context* ctx);

/**
 * @brief Initialize context with field offset data
 * @param ctx Context to initialize
 * @param field_items Array of field offset items
 * @param item_count Number of items in array
 * @param hash_table Hash table for lookups
 * @param hash_size Size of hash table
 * @return 0 on success, negative on error
 */
extern int json_context_init(struct json_context* ctx,
                     struct json_field_offset_item* field_items,
                     int item_count,
                     int* hash_table,
                     int hash_size);

/**
 * @brief Find field offset item in thread-safe manner
 * @param ctx JSON context
 * @param struct_name Structure name
 * @param field_name Field name
 * @return Pointer to field offset item, or NULL if not found
 */
extern struct json_field_offset_item* json_context_find_field(
    struct json_context* ctx,
    const char* struct_name,
    const char* field_name);

#endif /* JSON_CONTEXT_H */
