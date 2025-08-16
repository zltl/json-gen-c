/**
 * @file nested_struct_test.cc
 * @brief Test cases for nested and complex structures
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#include "json.gen.h"
#include "sstr.h"

class NestedStructTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

// Test nested structure marshaling and unmarshaling
TEST_F(NestedStructTest, BasicNestedStruct) {
    struct NestedStruct nested;
    NestedStruct_init(&nested);
    
    nested.id = 42;
    nested.name = sstr("Nested Test");
    
    // Initialize embedded TestStruct
    nested.embedded.int_val = 100;
    nested.embedded.long_val = 200L;
    nested.embedded.float_val = 3.14159f;
    nested.embedded.double_val = 2.718281828;
    nested.embedded.bool_val = true;
    nested.embedded.sstr_val = sstr("Embedded string");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_NestedStruct(&nested, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal nested struct";
    
    printf("Nested struct JSON: %s\n", sstr_cstr(json_out));
    
    struct NestedStruct unmarshaled;
    NestedStruct_init(&unmarshaled);
    result = json_unmarshal_NestedStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal nested struct";
    
    // Verify top-level fields
    EXPECT_EQ(nested.id, unmarshaled.id);
    EXPECT_TRUE(sstr_compare(nested.name, unmarshaled.name) == 0);
    
    // Verify embedded struct fields
    EXPECT_EQ(nested.embedded.int_val, unmarshaled.embedded.int_val);
    EXPECT_EQ(nested.embedded.long_val, unmarshaled.embedded.long_val);
    EXPECT_FLOAT_EQ(nested.embedded.float_val, unmarshaled.embedded.float_val);
    EXPECT_DOUBLE_EQ(nested.embedded.double_val, unmarshaled.embedded.double_val);
    EXPECT_EQ(nested.embedded.bool_val, unmarshaled.embedded.bool_val);
    EXPECT_TRUE(sstr_compare(nested.embedded.sstr_val, unmarshaled.embedded.sstr_val) == 0);
    
    NestedStruct_clear(&nested);
    NestedStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test complex structure with all types
TEST_F(NestedStructTest, ComplexStructAllTypes) {
    struct ComplexStruct complex;
    ComplexStruct_init(&complex);
    
    // Set simple fields
    complex.simple_int = 12345;
    complex.simple_long = 9876543210L;
    complex.simple_float = 1.23456f;
    complex.simple_double = 9.87654321;
    complex.simple_bool = true;
    complex.simple_string = sstr("Complex test string");
    
    // Set arrays
    complex.int_array_len = 3;
    complex.int_array = (int*)malloc(3 * sizeof(int));
    complex.int_array[0] = 10;
    complex.int_array[1] = 20;
    complex.int_array[2] = 30;
    
    complex.long_array_len = 2;
    complex.long_array = (long*)malloc(2 * sizeof(long));
    complex.long_array[0] = 1000000L;
    complex.long_array[1] = 2000000L;
    
    complex.float_array_len = 4;
    complex.float_array = (float*)malloc(4 * sizeof(float));
    complex.float_array[0] = 1.1f;
    complex.float_array[1] = 2.2f;
    complex.float_array[2] = 3.3f;
    complex.float_array[3] = 4.4f;
    
    complex.double_array_len = 2;
    complex.double_array = (double*)malloc(2 * sizeof(double));
    complex.double_array[0] = 10.123456789;
    complex.double_array[1] = 20.987654321;
    
    complex.string_array_len = 3;
    complex.string_array = (sstr_t*)malloc(3 * sizeof(sstr_t));
    complex.string_array[0] = sstr("First string");
    complex.string_array[1] = sstr("Second string");
    complex.string_array[2] = sstr("Third string");
    
    // Set nested address
    complex.address.number = sstr("456");
    complex.address.street = sstr("Complex Avenue");
    
    // Set contacts array
    complex.contacts_len = 2;
    complex.contacts = (struct Person*)malloc(2 * sizeof(struct Person));
    Person_init(&complex.contacts[0]);
    complex.contacts[0].name = sstr("Contact One");
    complex.contacts[0].age = sstr("25");
    Person_init(&complex.contacts[1]);
    complex.contacts[1].name = sstr("Contact Two");
    complex.contacts[1].age = sstr("30");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_ComplexStruct(&complex, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal complex struct";
    
    printf("Complex struct JSON: %s\n", sstr_cstr(json_out));
    
    struct ComplexStruct unmarshaled;
    ComplexStruct_init(&unmarshaled);
    result = json_unmarshal_ComplexStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal complex struct";
    
    // Verify simple fields
    EXPECT_EQ(complex.simple_int, unmarshaled.simple_int);
    EXPECT_EQ(complex.simple_long, unmarshaled.simple_long);
    EXPECT_FLOAT_EQ(complex.simple_float, unmarshaled.simple_float);
    EXPECT_DOUBLE_EQ(complex.simple_double, unmarshaled.simple_double);
    EXPECT_EQ(complex.simple_bool, unmarshaled.simple_bool);
    EXPECT_TRUE(sstr_compare(complex.simple_string, unmarshaled.simple_string) == 0);
    
    // Verify int array
    EXPECT_EQ(complex.int_array_len, unmarshaled.int_array_len);
    for (int i = 0; i < complex.int_array_len; i++) {
        EXPECT_EQ(complex.int_array[i], unmarshaled.int_array[i]);
    }
    
    // Verify long array
    EXPECT_EQ(complex.long_array_len, unmarshaled.long_array_len);
    for (int i = 0; i < complex.long_array_len; i++) {
        EXPECT_EQ(complex.long_array[i], unmarshaled.long_array[i]);
    }
    
    // Verify float array
    EXPECT_EQ(complex.float_array_len, unmarshaled.float_array_len);
    for (int i = 0; i < complex.float_array_len; i++) {
        EXPECT_FLOAT_EQ(complex.float_array[i], unmarshaled.float_array[i]);
    }
    
    // Verify double array
    EXPECT_EQ(complex.double_array_len, unmarshaled.double_array_len);
    for (int i = 0; i < complex.double_array_len; i++) {
        EXPECT_DOUBLE_EQ(complex.double_array[i], unmarshaled.double_array[i]);
    }
    
    // Verify string array
    EXPECT_EQ(complex.string_array_len, unmarshaled.string_array_len);
    for (int i = 0; i < complex.string_array_len; i++) {
        EXPECT_TRUE(sstr_compare(complex.string_array[i], unmarshaled.string_array[i]) == 0);
    }
    
    // Verify nested address
    EXPECT_TRUE(sstr_compare(complex.address.number, unmarshaled.address.number) == 0);
    EXPECT_TRUE(sstr_compare(complex.address.street, unmarshaled.address.street) == 0);
    
    // Verify contacts array
    EXPECT_EQ(complex.contacts_len, unmarshaled.contacts_len);
    for (int i = 0; i < complex.contacts_len; i++) {
        EXPECT_TRUE(sstr_compare(complex.contacts[i].name, unmarshaled.contacts[i].name) == 0);
        EXPECT_TRUE(sstr_compare(complex.contacts[i].age, unmarshaled.contacts[i].age) == 0);
    }
    
    ComplexStruct_clear(&complex);
    ComplexStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test edge case structure
TEST_F(NestedStructTest, EdgeCaseStruct) {
    struct EdgeCaseStruct edge;
    EdgeCaseStruct_init(&edge);
    
    edge.zero_int = 0;
    edge.negative_long = -9876543210L;
    edge.tiny_float = 1.175494e-38f;  // Near minimum positive float
    edge.huge_double = 1.7976931348623157e+308;  // Near maximum double
    edge.false_bool = false;
    edge.empty_string = sstr("");
    edge.special_chars_string = sstr("Special: \n\t\r\"\\");
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_EdgeCaseStruct(&edge, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal edge case struct";
    
    printf("Edge case struct JSON: %s\n", sstr_cstr(json_out));
    
    struct EdgeCaseStruct unmarshaled;
    EdgeCaseStruct_init(&unmarshaled);
    result = json_unmarshal_EdgeCaseStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal edge case struct";
    
    EXPECT_EQ(edge.zero_int, unmarshaled.zero_int);
    EXPECT_EQ(edge.negative_long, unmarshaled.negative_long);
    EXPECT_FLOAT_EQ(edge.tiny_float, unmarshaled.tiny_float);
    EXPECT_DOUBLE_EQ(edge.huge_double, unmarshaled.huge_double);
    EXPECT_EQ(edge.false_bool, unmarshaled.false_bool);
    EXPECT_TRUE(sstr_compare(edge.empty_string, unmarshaled.empty_string) == 0);
    EXPECT_TRUE(sstr_compare(edge.special_chars_string, unmarshaled.special_chars_string) == 0);
    
    EdgeCaseStruct_clear(&edge);
    EdgeCaseStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test optional fields handling
TEST_F(NestedStructTest, OptionalFieldsStruct) {
    struct OptionalFieldsStruct optional;
    OptionalFieldsStruct_init(&optional);
    
    optional.required_id = 999;
    optional.optional_name = sstr("Optional name");
    optional.optional_value = 99.99f;
    optional.has_data = true;
    
    sstr_t json_out = sstr_new();
    int result = json_marshal_OptionalFieldsStruct(&optional, json_out);
    ASSERT_EQ(result, 0) << "Failed to marshal optional fields struct";
    
    printf("Optional fields struct JSON: %s\n", sstr_cstr(json_out));
    
    struct OptionalFieldsStruct unmarshaled;
    OptionalFieldsStruct_init(&unmarshaled);
    result = json_unmarshal_OptionalFieldsStruct(json_out, &unmarshaled);
    ASSERT_EQ(result, 0) << "Failed to unmarshal optional fields struct";
    
    EXPECT_EQ(optional.required_id, unmarshaled.required_id);
    EXPECT_TRUE(sstr_compare(optional.optional_name, unmarshaled.optional_name) == 0);
    EXPECT_FLOAT_EQ(optional.optional_value, unmarshaled.optional_value);
    EXPECT_EQ(optional.has_data, unmarshaled.has_data);
    
    OptionalFieldsStruct_clear(&optional);
    OptionalFieldsStruct_clear(&unmarshaled);
    sstr_free(json_out);
}

// Test round-trip reliability with random data
TEST_F(NestedStructTest, RandomDataRoundTrip) {
    srand(time(NULL));
    
    for (int iteration = 0; iteration < 100; iteration++) {
        struct ComplexStruct complex;
        ComplexStruct_init(&complex);
        
        // Generate random data
        complex.simple_int = rand() - RAND_MAX/2;
        complex.simple_long = ((long)rand() << 32) | rand();
        complex.simple_float = (float)rand() / RAND_MAX * 1000.0f - 500.0f;
        complex.simple_double = (double)rand() / RAND_MAX * 1000000.0 - 500000.0;
        complex.simple_bool = rand() % 2;
        
        char random_string[100];
        snprintf(random_string, sizeof(random_string), "Random_%d_%d", iteration, rand());
        complex.simple_string = sstr(random_string);
        
        // Random int array
        int array_size = rand() % 10 + 1;
        complex.int_array_len = array_size;
        complex.int_array = (int*)malloc(array_size * sizeof(int));
        for (int i = 0; i < array_size; i++) {
            complex.int_array[i] = rand() - RAND_MAX/2;
        }
        
        // Set minimal nested data
        complex.address.number = sstr("123");
        complex.address.street = sstr("Test St");
        complex.contacts_len = 0;
        complex.contacts = NULL;
        
        // Test marshal/unmarshal
        sstr_t json_out = sstr_new();
        int result = json_marshal_ComplexStruct(&complex, json_out);
        ASSERT_EQ(result, 0) << "Failed to marshal random data iteration " << iteration;
        
        struct ComplexStruct unmarshaled;
        ComplexStruct_init(&unmarshaled);
        result = json_unmarshal_ComplexStruct(json_out, &unmarshaled);
        ASSERT_EQ(result, 0) << "Failed to unmarshal random data iteration " << iteration;
        
        // Verify key fields
        EXPECT_EQ(complex.simple_int, unmarshaled.simple_int) << "Int mismatch at iteration " << iteration;
        EXPECT_EQ(complex.simple_bool, unmarshaled.simple_bool) << "Bool mismatch at iteration " << iteration;
        EXPECT_TRUE(sstr_compare(complex.simple_string, unmarshaled.simple_string) == 0) 
            << "String mismatch at iteration " << iteration;
        
        // Verify array
        EXPECT_EQ(complex.int_array_len, unmarshaled.int_array_len) << "Array length mismatch at iteration " << iteration;
        for (int i = 0; i < complex.int_array_len; i++) {
            EXPECT_EQ(complex.int_array[i], unmarshaled.int_array[i]) 
                << "Array element mismatch at iteration " << iteration << ", index " << i;
        }
        
        ComplexStruct_clear(&complex);
        ComplexStruct_clear(&unmarshaled);
        sstr_free(json_out);
    }
}
