/**
 * @file comprehensive_test.cc
 * @brief Comprehensive test suite covering edge cases and complete functionality
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "json.gen.h"
#include "sstr.h"

class ComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test all primitive types with edge values
TEST_F(ComprehensiveTest, PrimitiveTypesEdgeCases) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    // Test with extreme values
    test_data.int_val = INT_MAX;
    test_data.long_val = LONG_MAX;
    test_data.float_val = FLT_MAX;
    test_data.double_val = DBL_MAX;
    test_data.bool_val = true;
    test_data.sstr_val = sstr("Maximum values test");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_TestStruct(&test_data, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal struct with max values";
    
    struct TestStruct unmarshaled;
    TestStruct_init(&unmarshaled);
    result = json_unmarshal_TestStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal struct with max values";
    
    EXPECT_EQ(test_data.int_val, unmarshaled.int_val);
    EXPECT_EQ(test_data.long_val, unmarshaled.long_val);
    EXPECT_EQ(test_data.bool_val, unmarshaled.bool_val);
    EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
    
    TestStruct_clear(&test_data);
    TestStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

TEST_F(ComprehensiveTest, PrimitiveTypesMinValues) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    // Test with minimum values
    test_data.int_val = INT_MIN;
    test_data.long_val = LONG_MIN;
    test_data.float_val = -FLT_MAX;
    test_data.double_val = -DBL_MAX;
    test_data.bool_val = false;
    test_data.sstr_val = sstr("Minimum values test");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_TestStruct(&test_data, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal struct with min values";
    
    struct TestStruct unmarshaled;
    TestStruct_init(&unmarshaled);
    result = json_unmarshal_TestStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal struct with min values";
    
    EXPECT_EQ(test_data.int_val, unmarshaled.int_val);
    EXPECT_EQ(test_data.long_val, unmarshaled.long_val);
    EXPECT_EQ(test_data.bool_val, unmarshaled.bool_val);
    EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
    
    TestStruct_clear(&test_data);
    TestStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test special float values
TEST_F(ComprehensiveTest, SpecialFloatValues) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    // Test special float values
    test_data.int_val = 0;
    test_data.long_val = 0;
    test_data.float_val = 0.0f;
    test_data.double_val = 0.0;
    test_data.bool_val = false;
    test_data.sstr_val = sstr("Zero values test");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_TestStruct(&test_data, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal struct with zero values";
    
    struct TestStruct unmarshaled;
    TestStruct_init(&unmarshaled);
    result = json_unmarshal_TestStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal struct with zero values";
    
    EXPECT_EQ(test_data.int_val, unmarshaled.int_val);
    EXPECT_EQ(test_data.long_val, unmarshaled.long_val);
    EXPECT_FLOAT_EQ(test_data.float_val, unmarshaled.float_val);
    EXPECT_DOUBLE_EQ(test_data.double_val, unmarshaled.double_val);
    EXPECT_EQ(test_data.bool_val, unmarshaled.bool_val);
    EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
    
    TestStruct_clear(&test_data);
    TestStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test arrays with various sizes
TEST_F(ComprehensiveTest, ArraySizeVariations) {
    // Test empty array
    {
        int *empty_array = NULL;
        int len = 0;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_int(empty_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal empty int array";
        
        int *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_int(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal empty int array";
        EXPECT_EQ(unmarshaled_len, 0);
        EXPECT_EQ(unmarshaled_array, nullptr);
        
        sstr_free(json_out);
    }
    
    // Test single element array
    {
        int single_array[] = {42};
        int len = 1;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_int(single_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal single element int array";
        
        int *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_int(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal single element int array";
        EXPECT_EQ(unmarshaled_len, 1);
        ASSERT_NE(unmarshaled_array, nullptr);
        EXPECT_EQ(unmarshaled_array[0], 42);
        
        free(unmarshaled_array);
        sstr_free(json_out);
    }
    
    // Test large array
    {
        const int large_size = 10000;
        int *large_array = (int*)malloc(large_size * sizeof(int));
        for (int i = 0; i < large_size; i++) {
            large_array[i] = i * 2;
        }
        
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_int(large_array, large_size, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal large int array";
        
        int *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_int(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal large int array";
        EXPECT_EQ(unmarshaled_len, large_size);
        ASSERT_NE(unmarshaled_array, nullptr);
        
        for (int i = 0; i < large_size; i++) {
            EXPECT_EQ(unmarshaled_array[i], i * 2) << "Mismatch at index " << i;
        }
        
        free(large_array);
        free(unmarshaled_array);
        sstr_free(json_out);
    }
}

// Test all primitive array types
TEST_F(ComprehensiveTest, AllPrimitiveArrayTypes) {
    // Test long array
    {
        long long_array[] = {LONG_MIN, -1000000L, 0L, 1000000L, LONG_MAX};
        int len = 5;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_long(long_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal long array";
        
        long *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_long(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal long array";
        EXPECT_EQ(unmarshaled_len, len);
        
        for (int i = 0; i < len; i++) {
            EXPECT_EQ(unmarshaled_array[i], long_array[i]);
        }
        
        free(unmarshaled_array);
        sstr_free(json_out);
    }
    
    // Test float array
    {
        float float_array[] = {-3.14159f, 0.0f, 2.71828f, FLT_MAX, -FLT_MAX};
        int len = 5;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_float(float_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal float array";
        
        float *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_float(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal float array";
        EXPECT_EQ(unmarshaled_len, len);
        
        for (int i = 0; i < len; i++) {
            EXPECT_FLOAT_EQ(unmarshaled_array[i], float_array[i]);
        }
        
        free(unmarshaled_array);
        sstr_free(json_out);
    }
    
    // Test double array
    {
        double double_array[] = {-3.141592653589793, 0.0, 2.718281828459045, DBL_MAX, -DBL_MAX};
        int len = 5;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_double(double_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal double array";
        
        double *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_double(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal double array";
        EXPECT_EQ(unmarshaled_len, len);
        
        for (int i = 0; i < len; i++) {
            EXPECT_DOUBLE_EQ(unmarshaled_array[i], double_array[i]);
        }
        
        free(unmarshaled_array);
        sstr_free(json_out);
    }
    
    // Test string array
    {
        sstr_t string_array[4];
        string_array[0] = sstr("Hello, World!");
        string_array[1] = sstr("");  // Empty string
        string_array[2] = sstr("Special chars: \n\t\r\"\\");
        string_array[3] = sstr("Unicode: 中文测试 🚀");
        
        int len = 4;
        sstr_t json_out = sstr_new();
        int result = json_marshal_array_sstr_t(string_array, len, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal string array";
        
        sstr_t *unmarshaled_array = NULL;
        int unmarshaled_len = 0;
        result = json_unmarshal_array_sstr_t(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0) << "Failed to unmarshal string array";
        EXPECT_EQ(unmarshaled_len, len);
        
        for (int i = 0; i < len; i++) {
            EXPECT_TRUE(sstr_compare(unmarshaled_array[i], string_array[i]) == 0);
            sstr_free(string_array[i]);
            sstr_free(unmarshaled_array[i]);
        }
        
        free(unmarshaled_array);
        sstr_free(json_out);
    }
}

// Test malformed JSON handling
TEST_F(ComprehensiveTest, MalformedJsonHandling) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    // Test various malformed JSON strings
    const char* malformed_jsons[] = {
        "{",                           // Incomplete object
        "{ \"int_val\": }",           // Missing value
        "{ \"int_val\": abc }",       // Invalid value
        "{ \"int_val\": 123, }",      // Trailing comma
        "{ \"unknown_field\": 123 }", // Unknown field only
        "[]",                         // Array instead of object
        "null",                       // Null instead of object
        "\"string\"",                 // String instead of object
        "{ \"int_val\": \"not_int\" }", // Wrong type
    };
    
    int num_malformed = sizeof(malformed_jsons) / sizeof(malformed_jsons[0]);
    
    for (int i = 0; i < num_malformed; i++) {
        sstr_t malformed_json = sstr(malformed_jsons[i]);
        int result = json_unmarshal_TestStruct(malformed_json, &test_data);
        EXPECT_NE(result, 0) << "Should fail for malformed JSON: " << malformed_jsons[i];
        sstr_free(malformed_json);
    }
    
    TestStruct_clear(&test_data);
}

// Test indented JSON output
TEST_F(ComprehensiveTest, IndentedJsonOutput) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    test_data.int_val = 42;
    test_data.long_val = 123456789L;
    test_data.float_val = 3.14159f;
    test_data.double_val = 2.718281828;
    test_data.bool_val = true;
    test_data.sstr_val = sstr("Indented test");
    
    // Test different indent levels
    for (int indent = 0; indent <= 8; indent += 2) {
        sstr_t json_out = sstr_new();
        int result = json_marshal_indent_TestStruct(&test_data, indent, 0, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal with indent " << indent;
        
        // Verify it can be unmarshaled back
        struct TestStruct unmarshaled;
        TestStruct_init(&unmarshaled);
        result = json_unmarshal_TestStruct(json_out, &unmarshaled);
        ASSERT_EQ(result, 0) << "Failed to unmarshal indented JSON with indent " << indent;
        
        EXPECT_EQ(test_data.int_val, unmarshaled.int_val);
        EXPECT_EQ(test_data.long_val, unmarshaled.long_val);
        EXPECT_FLOAT_EQ(test_data.float_val, unmarshaled.float_val);
        EXPECT_DOUBLE_EQ(test_data.double_val, unmarshaled.double_val);
        EXPECT_EQ(test_data.bool_val, unmarshaled.bool_val);
        EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
        
        TestStruct_clear(&unmarshaled);
        sstr_free(json_out);
    }
    
    TestStruct_clear(&test_data);
}

// Test nested struct arrays with various configurations
TEST_F(ComprehensiveTest, NestedStructArrayComplexCases) {
    // Test Data structure with various people array configurations
    
    // Case 1: Multiple people
    {
        struct Data data;
        Data_init(&data);
        
        data.house.number = sstr("123");
        data.house.street = sstr("Main Street");
        
        data.people_len = 3;
        data.people = (struct Person*)malloc(3 * sizeof(struct Person));
        
        Person_init(&data.people[0]);
        data.people[0].name = sstr("Alice");
        data.people[0].age = sstr("25");
        
        Person_init(&data.people[1]);
        data.people[1].name = sstr("Bob");
        data.people[1].age = sstr("30");
        
        Person_init(&data.people[2]);
        data.people[2].name = sstr("Charlie");
        data.people[2].age = sstr("35");
        
        sstr_t json_out = sstr_new();
        int result = json_marshal_Data(&data, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal Data with multiple people";
        
        struct Data unmarshaled;
        Data_init(&unmarshaled);
        result = json_unmarshal_Data(json_out, &unmarshaled);
        ASSERT_EQ(result, 0) << "Failed to unmarshal Data with multiple people";
        
        EXPECT_EQ(data.people_len, unmarshaled.people_len);
        EXPECT_TRUE(sstr_compare(data.house.number, unmarshaled.house.number) == 0);
        EXPECT_TRUE(sstr_compare(data.house.street, unmarshaled.house.street) == 0);
        
        for (int i = 0; i < data.people_len; i++) {
            EXPECT_TRUE(sstr_compare(data.people[i].name, unmarshaled.people[i].name) == 0);
            EXPECT_TRUE(sstr_compare(data.people[i].age, unmarshaled.people[i].age) == 0);
        }
        
        Data_clear(&data);
        Data_clear(&unmarshaled);
        sstr_free(json_out);
    }
}

// Test string edge cases
TEST_F(ComprehensiveTest, StringEdgeCases) {
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    
    const char* test_strings[] = {
        "",                                    // Empty string
        " ",                                   // Single space
        "   ",                                 // Multiple spaces
        "\n",                                  // Newline
        "\t",                                  // Tab
        "\r",                                  // Carriage return
        "\"",                                  // Quote
        "\\",                                  // Backslash
        "\"quoted string\"",                   // Quoted content
        "Line 1\nLine 2\nLine 3",             // Multi-line
        "Special: !@#$%^&*()_+-=[]{}|;':\",./<>?", // Special characters
        "Unicode: 中文测试 日本語 한국어 العربية 🚀🌟💫", // Unicode
    };
    
    int num_strings = sizeof(test_strings) / sizeof(test_strings[0]);
    
    for (int i = 0; i < num_strings; i++) {
        test_data.int_val = i;
        test_data.long_val = 0;
        test_data.float_val = 0.0f;
        test_data.double_val = 0.0;
        test_data.bool_val = false;
        if (test_data.sstr_val) {
            sstr_free(test_data.sstr_val);
        }
        test_data.sstr_val = sstr(test_strings[i]);
        
        sstr_t json_out = sstr_new();
        int result = json_marshal_TestStruct(&test_data, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal with string: " << test_strings[i];
        
        struct TestStruct unmarshaled;
        TestStruct_init(&unmarshaled);
        result = json_unmarshal_TestStruct(json_out, &unmarshaled);
        ASSERT_EQ(result, 0) << "Failed to unmarshal with string: " << test_strings[i];
        
        EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0)
            << "String mismatch for: " << test_strings[i];
        
        TestStruct_clear(&unmarshaled);
        sstr_free(json_out);
    }
    
    // Test the long string separately
    {
        std::string long_str = "Very long string: " + std::string(1000, 'x');
        test_data.sstr_val = sstr(long_str.c_str());
        
        sstr_t json_out = sstr_new();
        int result = json_marshal_TestStruct(&test_data, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal with long string";
        
        struct TestStruct unmarshaled;
        TestStruct_init(&unmarshaled);
        result = json_unmarshal_TestStruct(json_out, &unmarshaled);
        ASSERT_EQ(result, 0) << "Failed to unmarshal with long string";
        
        EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
        
        TestStruct_clear(&unmarshaled);
        sstr_free(json_out);
    }
    
    TestStruct_clear(&test_data);
}
