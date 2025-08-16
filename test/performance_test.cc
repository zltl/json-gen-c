/**
 * @file performance_test.cc
 * @brief Performance and stress test cases for json-gen-c
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <chrono>

#include "json.gen.h"
#include "sstr.h"

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    // Helper function to measure execution time
    template<typename Func>
    double measureTime(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0; // Return milliseconds
    }
};

// Test performance with large arrays
TEST_F(PerformanceTest, LargeIntArrayPerformance) {
    const int LARGE_SIZE = 100000;
    
    // Create large array
    int *large_array = (int*)malloc(LARGE_SIZE * sizeof(int));
    for (int i = 0; i < LARGE_SIZE; i++) {
        large_array[i] = i * 3 + 7; // Some pattern
    }
    
    sstr_t json_out = sstr_new();
    
    // Measure marshal time
    double marshal_time = measureTime([&]() {
        int result = json_marshal_array_int(large_array, LARGE_SIZE, json_out);
        ASSERT_EQ(result, 0);
    });
    
    printf("Marshal time for %d integers: %.2f ms\n", LARGE_SIZE, marshal_time);
    printf("JSON size: %zu bytes\n", sstr_length(json_out));
    
    // Measure unmarshal time
    int *unmarshaled_array = NULL;
    int unmarshaled_len = 0;
    
    double unmarshal_time = measureTime([&]() {
        int result = json_unmarshal_array_int(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0);
    });
    
    printf("Unmarshal time for %d integers: %.2f ms\n", LARGE_SIZE, unmarshal_time);
    
    // Verify correctness
    EXPECT_EQ(unmarshaled_len, LARGE_SIZE);
    for (int i = 0; i < LARGE_SIZE; i++) {
        EXPECT_EQ(large_array[i], unmarshaled_array[i]) << "Mismatch at index " << i;
    }
    
    free(large_array);
    free(unmarshaled_array);
    sstr_free(json_out);
}

// Test performance with large string arrays
TEST_F(PerformanceTest, LargeStringArrayPerformance) {
    const int STRING_COUNT = 10000;
    const int STRING_LENGTH = 50;
    
    // Create large string array
    sstr_t *string_array = (sstr_t*)malloc(STRING_COUNT * sizeof(sstr_t));
    for (int i = 0; i < STRING_COUNT; i++) {
        char buffer[STRING_LENGTH + 1];
        snprintf(buffer, sizeof(buffer), "String_number_%d_with_some_content", i);
        string_array[i] = sstr(buffer);
    }
    
    sstr_t json_out = sstr_new();
    
    // Measure marshal time
    double marshal_time = measureTime([&]() {
        int result = json_marshal_array_sstr_t(string_array, STRING_COUNT, json_out);
        ASSERT_EQ(result, 0);
    });
    
    printf("Marshal time for %d strings: %.2f ms\n", STRING_COUNT, marshal_time);
    printf("JSON size: %zu bytes\n", sstr_length(json_out));
    
    // Measure unmarshal time
    sstr_t *unmarshaled_array = NULL;
    int unmarshaled_len = 0;
    
    double unmarshal_time = measureTime([&]() {
        int result = json_unmarshal_array_sstr_t(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0);
    });
    
    printf("Unmarshal time for %d strings: %.2f ms\n", STRING_COUNT, unmarshal_time);
    
    // Verify correctness
    EXPECT_EQ(unmarshaled_len, STRING_COUNT);
    for (int i = 0; i < STRING_COUNT; i++) {
        EXPECT_TRUE(sstr_compare(string_array[i], unmarshaled_array[i]) == 0) 
            << "String mismatch at index " << i;
    }
    
    // Cleanup
    for (int i = 0; i < STRING_COUNT; i++) {
        sstr_free(string_array[i]);
        sstr_free(unmarshaled_array[i]);
    }
    free(string_array);
    free(unmarshaled_array);
    sstr_free(json_out);
}

// Test performance with large struct arrays
TEST_F(PerformanceTest, LargeStructArrayPerformance) {
    const int STRUCT_COUNT = 10000;
    
    // Create large struct array
    struct TestStruct *struct_array = (struct TestStruct*)malloc(STRUCT_COUNT * sizeof(struct TestStruct));
    for (int i = 0; i < STRUCT_COUNT; i++) {
        TestStruct_init(&struct_array[i]);
        struct_array[i].int_val = i;
        struct_array[i].long_val = i * 1000L;
        struct_array[i].float_val = i * 0.1f;
        struct_array[i].double_val = i * 0.001;
        struct_array[i].bool_val = i % 2;
        
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "TestStruct_%d", i);
        struct_array[i].sstr_val = sstr(buffer);
    }
    
    sstr_t json_out = sstr_new();
    
    // Measure marshal time
    double marshal_time = measureTime([&]() {
        int result = json_marshal_array_TestStruct(struct_array, STRUCT_COUNT, json_out);
        ASSERT_EQ(result, 0);
    });
    
    printf("Marshal time for %d TestStructs: %.2f ms\n", STRUCT_COUNT, marshal_time);
    printf("JSON size: %zu bytes\n", sstr_length(json_out));
    
    // Measure unmarshal time
    struct TestStruct *unmarshaled_array = NULL;
    int unmarshaled_len = 0;
    
    double unmarshal_time = measureTime([&]() {
        int result = json_unmarshal_array_TestStruct(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0);
    });
    
    printf("Unmarshal time for %d TestStructs: %.2f ms\n", STRUCT_COUNT, unmarshal_time);
    
    // Verify correctness (sample check)
    EXPECT_EQ(unmarshaled_len, STRUCT_COUNT);
    for (int i = 0; i < STRUCT_COUNT; i += 1000) { // Check every 1000th element
        EXPECT_EQ(struct_array[i].int_val, unmarshaled_array[i].int_val);
        EXPECT_EQ(struct_array[i].long_val, unmarshaled_array[i].long_val);
        EXPECT_FLOAT_EQ(struct_array[i].float_val, unmarshaled_array[i].float_val);
        EXPECT_DOUBLE_EQ(struct_array[i].double_val, unmarshaled_array[i].double_val);
        EXPECT_EQ(struct_array[i].bool_val, unmarshaled_array[i].bool_val);
        EXPECT_TRUE(sstr_compare(struct_array[i].sstr_val, unmarshaled_array[i].sstr_val) == 0);
    }
    
    // Cleanup
    for (int i = 0; i < STRUCT_COUNT; i++) {
        TestStruct_clear(&struct_array[i]);
        TestStruct_clear(&unmarshaled_array[i]);
    }
    free(struct_array);
    free(unmarshaled_array);
    sstr_free(json_out);
}

// Stress test with deep nesting
TEST_F(PerformanceTest, DeepNestingStressTest) {
    const int NESTING_DEPTH = 100;
    
    // Create deeply nested Data structures
    struct Data *nested_data = (struct Data*)malloc(NESTING_DEPTH * sizeof(struct Data));
    
    for (int i = 0; i < NESTING_DEPTH; i++) {
        Data_init(&nested_data[i]);
        
        char number_buf[20], street_buf[50];
        snprintf(number_buf, sizeof(number_buf), "%d", i + 1);
        snprintf(street_buf, sizeof(street_buf), "Street Level %d", i);
        
        nested_data[i].house.number = sstr(number_buf);
        nested_data[i].house.street = sstr(street_buf);
        
        // Add some people at each level
        int people_count = (i % 3) + 1; // 1-3 people per level
        nested_data[i].people_len = people_count;
        nested_data[i].people = (struct Person*)malloc(people_count * sizeof(struct Person));
        
        for (int j = 0; j < people_count; j++) {
            Person_init(&nested_data[i].people[j]);
            
            char name_buf[50], age_buf[20];
            snprintf(name_buf, sizeof(name_buf), "Person_%d_%d", i, j);
            snprintf(age_buf, sizeof(age_buf), "%d", 20 + i + j);
            
            nested_data[i].people[j].name = sstr(name_buf);
            nested_data[i].people[j].age = sstr(age_buf);
        }
    }
    
    // Marshal all nested data
    sstr_t json_out = sstr_new();
    
    double marshal_time = measureTime([&]() {
        int result = json_marshal_array_Data(nested_data, NESTING_DEPTH, json_out);
        ASSERT_EQ(result, 0);
    });
    
    printf("Marshal time for %d nested Data structures: %.2f ms\n", NESTING_DEPTH, marshal_time);
    printf("JSON size: %zu bytes\n", sstr_length(json_out));
    
    // Unmarshal
    struct Data *unmarshaled_array = NULL;
    int unmarshaled_len = 0;
    
    double unmarshal_time = measureTime([&]() {
        int result = json_unmarshal_array_Data(json_out, &unmarshaled_array, &unmarshaled_len);
        ASSERT_EQ(result, 0);
    });
    
    printf("Unmarshal time for %d nested Data structures: %.2f ms\n", NESTING_DEPTH, unmarshal_time);
    
    // Verify correctness
    EXPECT_EQ(unmarshaled_len, NESTING_DEPTH);
    for (int i = 0; i < NESTING_DEPTH; i++) {
        EXPECT_TRUE(sstr_compare(nested_data[i].house.number, unmarshaled_array[i].house.number) == 0);
        EXPECT_TRUE(sstr_compare(nested_data[i].house.street, unmarshaled_array[i].house.street) == 0);
        EXPECT_EQ(nested_data[i].people_len, unmarshaled_array[i].people_len);
        
        for (int j = 0; j < nested_data[i].people_len; j++) {
            EXPECT_TRUE(sstr_compare(nested_data[i].people[j].name, unmarshaled_array[i].people[j].name) == 0);
            EXPECT_TRUE(sstr_compare(nested_data[i].people[j].age, unmarshaled_array[i].people[j].age) == 0);
        }
    }
    
    // Cleanup
    for (int i = 0; i < NESTING_DEPTH; i++) {
        Data_clear(&nested_data[i]);
        Data_clear(&unmarshaled_array[i]);
    }
    free(nested_data);
    free(unmarshaled_array);
    sstr_free(json_out);
}

// Memory stress test
TEST_F(PerformanceTest, MemoryStressTest) {
    const int ITERATIONS = 1000;
    const int ARRAY_SIZE = 1000;
    
    printf("Running memory stress test with %d iterations...\n", ITERATIONS);
    
    double total_time = measureTime([&]() {
        for (int iter = 0; iter < ITERATIONS; iter++) {
            // Create array
            int *test_array = (int*)malloc(ARRAY_SIZE * sizeof(int));
            for (int i = 0; i < ARRAY_SIZE; i++) {
                test_array[i] = iter * ARRAY_SIZE + i;
            }
            
            // Marshal
            sstr_t json_out = sstr_new();
            int result = json_marshal_array_int(test_array, ARRAY_SIZE, json_out);
            ASSERT_EQ(result, 0);
            
            // Unmarshal
            int *unmarshaled_array = NULL;
            int unmarshaled_len = 0;
            result = json_unmarshal_array_int(json_out, &unmarshaled_array, &unmarshaled_len);
            ASSERT_EQ(result, 0);
            
            // Quick verification
            EXPECT_EQ(unmarshaled_len, ARRAY_SIZE);
            if (iter % 100 == 0) {
                for (int i = 0; i < ARRAY_SIZE; i++) {
                    EXPECT_EQ(test_array[i], unmarshaled_array[i]);
                }
            }
            
            // Cleanup
            free(test_array);
            free(unmarshaled_array);
            sstr_free(json_out);
            
            if (iter % 100 == 0) {
                printf("Completed %d/%d iterations\n", iter, ITERATIONS);
            }
        }
    });
    
    printf("Total time for %d iterations: %.2f ms\n", ITERATIONS, total_time);
    printf("Average time per iteration: %.2f ms\n", total_time / ITERATIONS);
}

// Test with very long strings
TEST_F(PerformanceTest, VeryLongStringTest) {
    const int STRING_LENGTH = 1000000; // 1MB string
    
    // Create very long string
    char *long_string = (char*)malloc(STRING_LENGTH + 1);
    for (int i = 0; i < STRING_LENGTH; i++) {
        long_string[i] = 'A' + (i % 26); // Repeating alphabet
    }
    long_string[STRING_LENGTH] = '\0';
    
    struct TestStruct test_data;
    TestStruct_init(&test_data);
    test_data.int_val = 12345;
    test_data.long_val = 9876543210L;
    test_data.float_val = 3.14159f;
    test_data.double_val = 2.718281828;
    test_data.bool_val = true;
    test_data.sstr_val = sstr(long_string);
    
    sstr_t json_out = sstr_new();
    
    double marshal_time = measureTime([&]() {
        int result = json_marshal_TestStruct(&test_data, json_out);
        ASSERT_EQ(result, 0);
    });
    
    printf("Marshal time for struct with %d-byte string: %.2f ms\n", STRING_LENGTH, marshal_time);
    printf("JSON size: %zu bytes\n", sstr_length(json_out));
    
    struct TestStruct unmarshaled;
    TestStruct_init(&unmarshaled);
    
    double unmarshal_time = measureTime([&]() {
        int result = json_unmarshal_TestStruct(json_out, &unmarshaled);
        ASSERT_EQ(result, 0);
    });
    
    printf("Unmarshal time for struct with %d-byte string: %.2f ms\n", STRING_LENGTH, unmarshal_time);
    
    // Verify correctness
    EXPECT_EQ(test_data.int_val, unmarshaled.int_val);
    EXPECT_EQ(test_data.long_val, unmarshaled.long_val);
    EXPECT_FLOAT_EQ(test_data.float_val, unmarshaled.float_val);
    EXPECT_DOUBLE_EQ(test_data.double_val, unmarshaled.double_val);
    EXPECT_EQ(test_data.bool_val, unmarshaled.bool_val);
    EXPECT_TRUE(sstr_compare(test_data.sstr_val, unmarshaled.sstr_val) == 0);
    
    free(long_string);
    TestStruct_clear(&test_data);
    TestStruct_clear(&unmarshaled);
    sstr_free(json_out);
}
