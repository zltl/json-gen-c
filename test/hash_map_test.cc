#include <gtest/gtest.h>
#include "utils/hash_map.h"
#include "utils/sstr.h"

// Helper functions for hash map
unsigned int test_hash_func(void* key) {
    sstr_t s = (sstr_t)key;
    const char* str = sstr_cstr(s);
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

int test_key_cmp(void* a, void* b) {
    return sstr_compare((sstr_t)a, (sstr_t)b);
}

void test_key_free(void* key) {
    sstr_free((sstr_t)key);
}

void test_value_free(void* value) {
    // Do nothing for integer values cast to void*
    (void)value;
}

TEST(HashMapTest, ResizeTest) {
    // Create a map with small bucket count
    int initial_buckets = 4;
    struct hash_map* map = hash_map_new(initial_buckets, test_hash_func, test_key_cmp, test_key_free, test_value_free);
    ASSERT_NE(map, nullptr);
    ASSERT_EQ(map->bucket_count, initial_buckets);

    // Insert elements to trigger resize
    // Threshold is 0.75 * 4 = 3. So inserting 4th element should trigger resize (or check logic)
    // Let's insert 10 elements to be sure
    for (int i = 0; i < 10; ++i) {
        char buf[32];
        sprintf(buf, "key_%d", i);
        sstr_t key = sstr(buf);
        // Use i+1 as value to avoid NULL (0)
        intptr_t value = i + 1;
        
        int res = hash_map_insert(map, key, (void*)value);
        ASSERT_EQ(res, HASH_MAP_OK);
    }

    // Check if resized
    // If logic is correct, it should have doubled at least once or twice
    // 4 -> 8 -> 16
    EXPECT_GT(map->bucket_count, initial_buckets);
    
    // Verify all elements are still there
    for (int i = 0; i < 10; ++i) {
        char buf[32];
        sprintf(buf, "key_%d", i);
        sstr_t key = sstr(buf); // Need a new sstr for lookup key, but we need to free it
        
        void* val_ptr = NULL;
        int res = hash_map_find(map, key, &val_ptr);
        EXPECT_EQ(res, HASH_MAP_OK) << "Failed to find key: " << buf;
        EXPECT_EQ((intptr_t)val_ptr, i + 1);
        
        sstr_free(key);
    }

    hash_map_free(map);
}
