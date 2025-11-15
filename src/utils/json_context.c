/**
 * @file json_context.c
 * @brief Implementation of thread-safe JSON parsing context
 * 
 * Provides thread-safe management of JSON field offset tables and
 * hash structures. The implementation uses POSIX mutexes to protect
 * concurrent access to shared context data.
 */

#include "json_context.h"
#include "error_codes.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a new JSON parsing context
 * 
 * Implementation details:
 * 1. Allocates memory for context structure
 * 2. Zeros all fields
 * 3. Initializes pthread mutex
 * 4. Returns context ready for initialization
 */
struct json_context* json_context_new(void) {
    struct json_context* ctx = malloc(sizeof(struct json_context));
    if (ctx == NULL) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(struct json_context));
    
    // Initialize mutex
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

/**
 * @brief Free a JSON parsing context
 * 
 * Destroys the mutex and frees the context structure. Does not free
 * the field_offset_items or entry_hash arrays as these are typically
 * static data owned by generated code.
 * 
 * @param ctx Context to free (safe to pass NULL)
 */
void json_context_free(struct json_context* ctx) {
    if (ctx == NULL) {
        return;
    }
    
    pthread_mutex_destroy(&ctx->mutex);
    
    // Note: We don't free field_offset_items and entry_hash here
    // as they are typically static/global data owned by the generated code
    
    free(ctx);
}

/**
 * @brief Initialize context with field offset data
 * 
 * Thread-safe initialization that associates the context with field
 * offset arrays and hash table. The context stores pointers to these
 * arrays without copying them.
 * 
 * @param ctx Context to initialize
 * @param field_items Field offset array (must remain valid)
 * @param item_count Array size
 * @param hash_table Hash table for lookups (must remain valid)
 * @param hash_size Hash table size
 * @return JSON_GEN_SUCCESS or error code
 */
int json_context_init(struct json_context* ctx,
                     struct json_field_offset_item* field_items,
                     int item_count,
                     int* hash_table,
                     int hash_size) {
    if (ctx == NULL || field_items == NULL || hash_table == NULL) {
        return JSON_GEN_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    ctx->field_offset_items = field_items;
    ctx->item_count = item_count;
    ctx->entry_hash = hash_table;
    ctx->entry_hash_size = hash_size;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return JSON_GEN_SUCCESS;
}

/**
 * @brief Hash function for two C strings concatenated with separator
 * 
 * Computes a hash value from two strings joined with '#' separator.
 * Used to create unique keys from struct name and field name pairs.
 * The hash algorithm is a simple multiplicative hash.
 * 
 * @param key1 First string (typically struct name)
 * @param key2 Second string (typically field name)
 * @return 32-bit hash value
 */
static unsigned int hash_2s_c(const char* key1, const char* key2) {
    unsigned int res = 0xbc9f1d34;
    const unsigned int m = 0xc6a4a793;
    
    // Hash first string
    const char* p = key1;
    while (*p) {
        res = res * m + (unsigned char)*p;
        p++;
    }
    
    // Add separator
    res = res * m + '#';
    
    // Hash second string
    p = key2;
    while (*p) {
        res = res * m + (unsigned char)*p;
        p++;
    }
    
    return res;
}

struct json_field_offset_item* json_context_find_field(
    struct json_context* ctx,
    const char* struct_name,
    const char* field_name) {
    
    if (ctx == NULL || struct_name == NULL || field_name == NULL) {
        return NULL;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (ctx->entry_hash_size == 0 || ctx->entry_hash == NULL || 
        ctx->field_offset_items == NULL) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }
    
    unsigned int h = hash_2s_c(struct_name, field_name) % ctx->entry_hash_size;
    int id = ctx->entry_hash[h];
    
    if (id < 0) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }

    do {
        struct json_field_offset_item* item = &ctx->field_offset_items[id];
        if (strcmp(struct_name, item->struct_name) == 0 &&
            strcmp(field_name, item->field_name) == 0) {
            pthread_mutex_unlock(&ctx->mutex);
            return item;
        }
        
        h++;
        if ((int)h >= ctx->entry_hash_size) {
            h = 0;
        }
        id = ctx->entry_hash[h];
        
        // -1 means empty slot
        if (id < 0) {
            break;
        }
    } while (1);

    pthread_mutex_unlock(&ctx->mutex);
    return NULL;
}
