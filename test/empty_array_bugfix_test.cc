#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.gen.h"
#include "sstr.h"

// Test fixture for empty array bug fix
class EmptyArrayBugFixTest : public ::testing::Test {
protected:
    void SetUp() override {
        // This will be called before each test
    }

    void TearDown() override {
        // This will be called after each test
    }
};

// Test that empty arrays of structs are correctly handled
TEST_F(EmptyArrayBugFixTest, EmptyStructArrayUnmarshal) {
    const char* empty_json = R"({
"house":{
"number":"288",
"street":"1st ave."
},
"people":[
]
})";

    struct Data data;
    Data_init(&data);
    
    sstr_t json_str = sstr(empty_json);
    int result = json_unmarshal_Data(json_str, &data);
    
    ASSERT_GE(result, 0) << "Failed to parse empty array JSON";
    EXPECT_EQ(data.people_len, 0) << "Empty array should have length 0";
    EXPECT_EQ(data.people, nullptr) << "Empty array should have null pointer";
    
    // Verify house data was parsed correctly
    EXPECT_STREQ(sstr_cstr(data.house.number), "288");
    EXPECT_STREQ(sstr_cstr(data.house.street), "1st ave.");
    
    Data_clear(&data);
    sstr_free(json_str);
}

// Test that non-empty arrays still work correctly
TEST_F(EmptyArrayBugFixTest, NonEmptyStructArrayUnmarshal) {
    const char* non_empty_json = R"({
"house":{
"number":"288",
"street":"1st ave."
},
"people":[
{
"name":"dogleft",
"age":"8"
}
]
})";

    struct Data data;
    Data_init(&data);
    
    sstr_t json_str = sstr(non_empty_json);
    int result = json_unmarshal_Data(json_str, &data);
    
    ASSERT_GE(result, 0) << "Failed to parse non-empty array JSON";
    EXPECT_EQ(data.people_len, 1) << "Non-empty array should have length 1";
    ASSERT_NE(data.people, nullptr) << "Non-empty array should not have null pointer";
    
    // Verify person data
    EXPECT_STREQ(sstr_cstr(data.people[0].name), "dogleft");
    EXPECT_STREQ(sstr_cstr(data.people[0].age), "8");
    
    // Verify house data
    EXPECT_STREQ(sstr_cstr(data.house.number), "288");
    EXPECT_STREQ(sstr_cstr(data.house.street), "1st ave.");
    
    Data_clear(&data);
    sstr_free(json_str);
}

// Test round-trip: marshal -> unmarshal -> marshal for empty arrays
TEST_F(EmptyArrayBugFixTest, EmptyArrayRoundTrip) {
    // Create data with empty array
    struct Data original_data;
    Data_init(&original_data);
    
    original_data.house.number = sstr("288");
    original_data.house.street = sstr("1st ave.");
    original_data.people_len = 0;
    original_data.people = nullptr;
    
    // Marshal to JSON
    sstr_t json_out = sstr_new();
    json_marshal_indent_Data(&original_data, 4, 0, json_out);
    
    // Unmarshal back
    struct Data result_data;
    Data_init(&result_data);
    int result = json_unmarshal_Data(json_out, &result_data);
    
    ASSERT_GE(result, 0) << "Failed to unmarshal JSON";
    EXPECT_EQ(result_data.people_len, 0) << "Round-trip should preserve empty array";
    EXPECT_EQ(result_data.people, nullptr) << "Round-trip should preserve null pointer";
    
    // Marshal again to verify consistency
    sstr_t json_out2 = sstr_new();
    json_marshal_indent_Data(&result_data, 4, 0, json_out2);
    
    // The JSON should be consistent
    EXPECT_STREQ(sstr_cstr(json_out), sstr_cstr(json_out2)) << "Round-trip should produce identical JSON";
    
    // Clean up
    Data_clear(&original_data);
    Data_clear(&result_data);
    sstr_free(json_out);
    sstr_free(json_out2);
}

// Test that multiple empty arrays work correctly
TEST_F(EmptyArrayBugFixTest, MultipleEmptyArrays) {
    const char* multi_empty_json = R"({
"house":{
"number":"288",
"street":"1st ave."
},
"people":[
]
})";

    // Test multiple consecutive unmarshals
    for (int i = 0; i < 3; i++) {
        struct Data data;
        Data_init(&data);
        
        sstr_t json_str = sstr(multi_empty_json);
        int result = json_unmarshal_Data(json_str, &data);
        
        ASSERT_GE(result, 0) << "Failed to parse JSON on iteration " << i;
        EXPECT_EQ(data.people_len, 0) << "Empty array should have length 0 on iteration " << i;
        
        Data_clear(&data);
        sstr_free(json_str);
    }
}

// Test edge case: array with only whitespace
TEST_F(EmptyArrayBugFixTest, EmptyArrayWithWhitespace) {
    const char* whitespace_json = R"({
"house":{
"number":"288",
"street":"1st ave."
},
"people":[   
  
  
]
})";

    struct Data data;
    Data_init(&data);
    
    sstr_t json_str = sstr(whitespace_json);
    int result = json_unmarshal_Data(json_str, &data);
    
    ASSERT_GE(result, 0) << "Failed to parse JSON with whitespace in empty array";
    EXPECT_EQ(data.people_len, 0) << "Empty array with whitespace should have length 0";
    
    Data_clear(&data);
    sstr_free(json_str);
}

// Test that the original bug scenario fails without the fix
TEST_F(EmptyArrayBugFixTest, VerifyBugWasFixed) {
    const char* empty_json = R"({"house":{"number":"288","street":"1st ave."},"people":[]})";
    
    struct Data data;
    Data_init(&data);
    
    sstr_t json_str = sstr(empty_json);
    int result = json_unmarshal_Data(json_str, &data);
    
    ASSERT_GE(result, 0) << "Failed to parse empty array JSON";
    
    // The bug would have created 1 element with empty strings
    // With the fix, we should have 0 elements
    EXPECT_EQ(data.people_len, 0) << "BUG FIXED: Empty array should not create spurious elements";
    
    // Verify no memory was allocated for the array
    EXPECT_EQ(data.people, nullptr) << "BUG FIXED: Empty array should not allocate memory";
    
    Data_clear(&data);
    sstr_free(json_str);
}
