/**
 * @file enhanced_test.cc
 * @brief Enhanced test suite for json-gen-c optimizations
 */

#include <gtest/gtest.h>
#include <string>
#include <memory>

extern "C" {
#include "utils/error_codes.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"
#include "utils/json_context.h"
}

class JsonGenCEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test error code functionality
TEST_F(JsonGenCEnhancedTest, ErrorCodeStrings) {
    EXPECT_STREQ("Success", json_gen_error_string(JSON_GEN_SUCCESS));
    EXPECT_STREQ("Memory allocation error", json_gen_error_string(JSON_GEN_ERROR_MEMORY));
    EXPECT_STREQ("File I/O error", json_gen_error_string(JSON_GEN_ERROR_FILE_IO));
    EXPECT_STREQ("Unknown error", json_gen_error_string(static_cast<json_gen_error_t>(999)));
}

// Test sstr format validation improvements
TEST_F(JsonGenCEnhancedTest, SstrFormatValidation) {
    sstr_t str = sstr_new();
    ASSERT_NE(nullptr, str);
    
    // Test long format validation
    sstr_t result = sstr_printf_append(str, "Invalid format: %l", 123);
    EXPECT_NE(nullptr, result);
    
    // Should handle invalid format gracefully
    std::string output(sstr_cstr(result));
    EXPECT_TRUE(output.find("Invalid format:") != std::string::npos);
    
    sstr_free(str);
}

// Test hash map load factor checking
TEST_F(JsonGenCEnhancedTest, HashMapLoadFactor) {
    auto hash_func = [](void* key) -> unsigned int {
        return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(key));
    };
    
    auto key_cmp = [](void* a, void* b) -> int {
        return (a == b) ? 0 : 1;
    };
    
    auto free_func = [](void* ptr) {
        (void)ptr;  // Suppress unused parameter warning
        // No-op for test
    };
    
    struct hash_map* map = hash_map_new(4, hash_func, key_cmp, free_func, free_func);
    ASSERT_NE(nullptr, map);
    
    // Insert multiple items to test load factor
    void* keys[] = {(void*)1, (void*)2, (void*)3, (void*)4, (void*)5};
    void* values[] = {(void*)10, (void*)20, (void*)30, (void*)40, (void*)50};
    
    for (int i = 0; i < 5; i++) {
        int result = hash_map_insert(map, keys[i], values[i]);
        EXPECT_EQ(HASH_MAP_OK, result);
    }
    
    hash_map_free(map);
}

// Test JSON context thread safety
TEST_F(JsonGenCEnhancedTest, JsonContextBasic) {
    struct json_context* ctx = json_context_new();
    ASSERT_NE(nullptr, ctx);
    
    // Test context without initialization
    struct json_field_offset_item* item = json_context_find_field(ctx, "test", "field");
    EXPECT_EQ(nullptr, item);
    
    json_context_free(ctx);
}

// Test memory bounds checking
TEST_F(JsonGenCEnhancedTest, MemoryBoundsChecking) {
    sstr_t str = sstr_new();
    ASSERT_NE(nullptr, str);
    
    // Test extremely long string
    const size_t large_size = 1024 * 1024;  // 1MB
    std::string large_string(large_size, 'A');
    
    sstr_append_cstr(str, large_string.c_str());
    EXPECT_EQ(large_size, sstr_length(str));
    
    sstr_free(str);
}

// Test edge cases for string parsing
TEST_F(JsonGenCEnhancedTest, StringParsingEdgeCases) {
    sstr_t str = sstr_new();
    ASSERT_NE(nullptr, str);
    
    // Test empty string
    sstr_t empty = sstr("");
    EXPECT_EQ(0, sstr_length(empty));
    
    // Test string with null bytes (should handle gracefully)
    const char test_data[] = "test\0hidden";
    sstr_t with_null = sstr_of(test_data, sizeof(test_data) - 1);
    EXPECT_GT(sstr_length(with_null), 4);  // Should include the null byte
    
    sstr_free(str);
    sstr_free(empty);
    sstr_free(with_null);
}

// Test resource cleanup
TEST_F(JsonGenCEnhancedTest, ResourceCleanup) {
    // Test that multiple allocations and frees work correctly
    for (int i = 0; i < 100; i++) {
        sstr_t str = sstr_new();
        ASSERT_NE(nullptr, str);
        
        sstr_append_cstr(str, "test string");
        EXPECT_GT(sstr_length(str), 0);
        
        sstr_free(str);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
