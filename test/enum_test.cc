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

// --- Boundary semantics tests ---

TEST_F(EnumTest, UnmarshalNegativeIntEnum) {
    // Negative integer for enum should still be accepted (stored as-is)
    const char* json_str = "{\"color\":-1,\"status\":0,\"value\":0,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.color, -1);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, MarshalNegativeEnumFallsBackToInt) {
    // Negative enum value should marshal as integer, not crash
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = -5;
    obj.status = Status_ACTIVE;
    obj.value = 0;

    sstr_t out = sstr_new();
    json_marshal_EnumTestStruct(&obj, out);

    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "-5") != NULL) << "Expected -5 in: " << json;

    sstr_free(out);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalEnumArrayWithIntegers) {
    // Integer values in enum array should work
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[0,1,2]}";
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

TEST_F(EnumTest, UnmarshalMixedStringIntEnumArray) {
    // Mixed strings and ints in enum array
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[\"RED\",1,\"BLUE\"]}";
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

TEST_F(EnumTest, UnmarshalNullEnumArray) {
    // null for enum array should set it to NULL/0
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":null}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.colors, nullptr);
    EXPECT_EQ(obj.colors_len, 0);

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalBoolForEnumFails) {
    // Boolean value for an enum field should fail
    const char* json_str = "{\"color\":true,\"status\":\"ACTIVE\",\"value\":0,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_NE(r, 0) << "Should fail for boolean enum value";

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, UnmarshalNullForScalarEnumFails) {
    // null for a scalar enum field should fail
    const char* json_str = "{\"color\":null,\"status\":\"ACTIVE\",\"value\":0,\"colors\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    int r = json_unmarshal_EnumTestStruct(input, &obj);
    EXPECT_NE(r, 0) << "Should fail for null enum value";

    sstr_free(input);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, MarshalAllEnumValues) {
    // Ensure all enum values marshal to correct strings
    const char* expected_colors[] = {"RED", "GREEN", "BLUE"};
    for (int i = 0; i < 3; i++) {
        struct EnumTestStruct obj;
        EnumTestStruct_init(&obj);
        obj.color = i;
        obj.status = Status_ACTIVE;
        obj.value = 0;

        sstr_t out = sstr_new();
        json_marshal_EnumTestStruct(&obj, out);
        const char* json = sstr_cstr(out);

        char expected[32];
        snprintf(expected, sizeof(expected), "\"%s\"", expected_colors[i]);
        EXPECT_TRUE(strstr(json, expected) != NULL)
            << "Expected " << expected << " in: " << json;

        sstr_free(out);
        EnumTestStruct_clear(&obj);
    }
}

TEST_F(EnumTest, EnumInitZero) {
    // After init, enum fields should be 0
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    EXPECT_EQ(obj.color, 0);
    EXPECT_EQ(obj.status, 0);
    EXPECT_EQ(obj.colors, nullptr);
    EXPECT_EQ(obj.colors_len, 0);
    EnumTestStruct_clear(&obj);
}

TEST_F(EnumTest, EnumClearResetsToZero) {
    // After clear, enum fields should be reset to 0
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = Color_BLUE;
    obj.status = Status_PENDING;
    obj.value = 42;

    // Unmarshal to get a heap-allocated colors array
    const char* json_str = "{\"color\":\"RED\",\"status\":\"ACTIVE\",\"value\":0,\"colors\":[\"RED\",\"GREEN\"]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));
    json_unmarshal_EnumTestStruct(input, &obj);
    sstr_free(input);

    EXPECT_EQ(obj.colors_len, 2);

    EnumTestStruct_clear(&obj);
    EXPECT_EQ(obj.color, 0);
    EXPECT_EQ(obj.status, 0);
    EXPECT_EQ(obj.colors, nullptr);
    EXPECT_EQ(obj.colors_len, 0);
}

// --- Generated code structure assertions ---

TEST_F(EnumTest, GeneratedEnumStringArraysExist) {
    // Verify the generated enum string arrays produce correct marshal output
    // for every enum value (tests that the arrays are correctly generated)
    const char* color_names[] = {"RED", "GREEN", "BLUE"};
    const char* status_names[] = {"ACTIVE", "INACTIVE", "PENDING"};

    for (int i = 0; i < 3; i++) {
        struct EnumTestStruct obj;
        EnumTestStruct_init(&obj);
        obj.color = i;
        obj.status = i;
        obj.value = 0;

        sstr_t out = sstr_new();
        json_marshal_EnumTestStruct(&obj, out);
        const char* json = sstr_cstr(out);

        char expected_c[32], expected_s[32];
        snprintf(expected_c, sizeof(expected_c), "\"%s\"", color_names[i]);
        snprintf(expected_s, sizeof(expected_s), "\"%s\"", status_names[i]);
        EXPECT_TRUE(strstr(json, expected_c) != NULL)
            << "Expected " << expected_c << " in: " << json;
        EXPECT_TRUE(strstr(json, expected_s) != NULL)
            << "Expected " << expected_s << " in: " << json;

        sstr_free(out);
        EnumTestStruct_clear(&obj);
    }
}

TEST_F(EnumTest, EnumFieldStoredAsInt) {
    // Verify enum fields occupy sizeof(int) in the struct
    EXPECT_EQ(sizeof(((struct EnumTestStruct*)0)->color), sizeof(int));
    EXPECT_EQ(sizeof(((struct EnumTestStruct*)0)->status), sizeof(int));
}

TEST_F(EnumTest, LargeEnumArrayRoundTrip) {
    // Build a large enum array and verify round trip
    struct EnumTestStruct obj;
    EnumTestStruct_init(&obj);
    obj.color = Color_RED;
    obj.status = Status_ACTIVE;
    obj.value = 0;

    const int N = 1000;
    int* colors = (int*)malloc(sizeof(int) * N);
    for (int i = 0; i < N; i++) {
        colors[i] = i % 3; // cycle RED, GREEN, BLUE
    }
    obj.colors = colors;
    obj.colors_len = N;

    sstr_t out = sstr_new();
    json_marshal_EnumTestStruct(&obj, out);

    // Reset before clear to avoid freeing stack memory
    obj.colors = NULL;
    obj.colors_len = 0;

    struct EnumTestStruct obj2;
    EnumTestStruct_init(&obj2);
    int r = json_unmarshal_EnumTestStruct(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.colors_len, N);
    for (int i = 0; i < N; i++) {
        EXPECT_EQ(obj2.colors[i], i % 3) << "Mismatch at index " << i;
    }

    free(colors);
    sstr_free(out);
    EnumTestStruct_clear(&obj);
    EnumTestStruct_clear(&obj2);
}
