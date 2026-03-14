extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>
#include <cstring>

class EnumTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EnumTest, MarshalScalarEnum) {
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = Color_GREEN;
    obj.status = Status_PENDING;
    obj.value = 42;

    sstr_t out = sstr_new();
    json_marshal_EnumTestStruct(&obj, out);

    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "\"GREEN\"") != NULL) << "Expected GREEN in: " << json;
    EXPECT_TRUE(strstr(json, "\"PENDING\"") != NULL) << "Expected PENDING in: " << json;
    EXPECT_TRUE(strstr(json, "42") != NULL) << "Expected 42 in: " << json;

    sstr_free(out);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalScalarEnum) {
    sstr_t input = sstr_of(
        "{\"color\":\"BLUE\",\"status\":\"ACTIVE\",\"value\":100,\"colors\":[]}",
        strlen("{\"color\":\"BLUE\",\"status\":\"ACTIVE\",\"value\":100,\"colors\":[]}"));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.color, Color_BLUE);
    EXPECT_EQ(obj.status, Status_ACTIVE);
    EXPECT_EQ(obj.value, 100);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, MarshalEnumArray) {
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = Color_RED;
    obj.status = Status_INACTIVE;
    obj.value = 10;

    int colors[] = {Color_RED, Color_GREEN, Color_BLUE};
    obj.colors = colors;
    obj.colors_len = 3;

    sstr_t out = sstr_new();
    json_marshal_EnumTestStruct(&obj, out);

    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "\"RED\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"GREEN\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"BLUE\"") != NULL) << "json: " << json;

    sstr_free(out);
    // Don't clear since colors is stack-allocated
    obj.colors = NULL;
    obj.colors_len = 0;
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalEnumArray) {
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[\"RED\",\"GREEN\",\"BLUE\"]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.colors_len, 3);
    EXPECT_EQ(obj.colors[0], Color_RED);
    EXPECT_EQ(obj.colors[1], Color_GREEN);
    EXPECT_EQ(obj.colors[2], Color_BLUE);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, RoundTrip) {
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = Color_BLUE;
    obj.status = Status_INACTIVE;
    obj.value = 99;

    int colors[] = {Color_GREEN, Color_RED};
    obj.colors = colors;
    obj.colors_len = 2;

    sstr_t json = sstr_new();
    json_marshal_EnumTestStruct(&obj, json);

    // Reset obj.colors before clear to avoid freeing stack memory
    obj.colors = NULL;
    obj.colors_len = 0;

    struct EnumTestStruct obj2;
    EnumTestStruct_init(&obj2);
    int r = json_unmarshal_EnumTestStruct(json, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.color, Color_BLUE);
    EXPECT_EQ(obj2.status, Status_INACTIVE);
    EXPECT_EQ(obj2.value, 99);
    EXPECT_EQ(obj2.colors_len, 2);
    EXPECT_EQ(obj2.colors[0], Color_GREEN);
    EXPECT_EQ(obj2.colors[1], Color_RED);

    sstr_free(json);
    EnumTestStruct_clear(&obj);
    EnumTestStruct_clear(&obj2);
}

TEST_F(EnumTest, UnmarshalEnumAsInt) {
    // Test that integer values also work for enum unmarshal
    const char* json_str = "{\"color\":1,\"status\":2,\"value\":5,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.color, Color_GREEN);   // GREEN via int
    EXPECT_EQ(obj.status, Status_PENDING);  // PENDING via int
    EXPECT_EQ(obj.value, 5);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalUnknownEnumValue) {
    // Unknown string value should return an error
    const char* json_str = "{\"color\":\"YELLOW\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_NE(r, 0) << "Should fail for unknown enum value YELLOW";

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, MarshalOutOfRangeEnum) {
    // Enum value out of range should fall back to integer output
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = 99;  // out of range
    obj.status = Status_ACTIVE;
    obj.value = 1;

    sstr_t out = sstr_new();
    json_marshal_EnumTestStruct(&obj, out);

    const char* json = sstr_cstr(out);
    // Should contain 99 as an integer, not a string
    EXPECT_TRUE(strstr(json, "99") != NULL) << "Expected 99 in: " << json;
    // status should still be a string
    EXPECT_TRUE(strstr(json, "\"ACTIVE\"") != NULL) << "Expected ACTIVE in: " << json;

    sstr_free(out);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalEnumArrayWithUnknownValue) {
    // Array containing invalid enum string should fail
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[\"RED\",\"PURPLE\"]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_NE(r, 0) << "Should fail for unknown array enum value PURPLE";

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, GeneratedEnumConstants) {
    // Verify the generated enum constants have correct values
    EXPECT_EQ(Color_RED, 0);
    EXPECT_EQ(Color_GREEN, 1);
    EXPECT_EQ(Color_BLUE, 2);
    EXPECT_EQ(Status_ACTIVE, 0);
    EXPECT_EQ(Status_INACTIVE, 1);
    EXPECT_EQ(Status_PENDING, 2);
}

TEST_F(EnumTest, EmptyEnumArray) {
    // Empty array of enums should work
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.colors_len, 0);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}
