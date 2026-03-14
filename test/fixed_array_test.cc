#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "json.gen.h"
}

class FixedArrayTest : public ::testing::Test {
protected:
    struct FixedArrayStruct obj;
    void SetUp() override {
        FixedArrayStruct_init(&obj);
    }
    void TearDown() override {
        FixedArrayStruct_clear(&obj);
    }
};

// =============================================================================
// Init / Clear
// =============================================================================

TEST_F(FixedArrayTest, InitZerosAllFields) {
    for (int i = 0; i < 5; i++) EXPECT_EQ(obj.fixed_ints[i], 0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(obj.fixed_longs[i], 0);
    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(obj.fixed_floats[i], 0.0f);
    for (int i = 0; i < 3; i++) EXPECT_DOUBLE_EQ(obj.fixed_doubles[i], 0.0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(obj.fixed_strings[i], nullptr);
    for (int i = 0; i < 2; i++) EXPECT_EQ(obj.fixed_bools[i], 0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(obj.fixed_colors[i], 0);
}

TEST_F(FixedArrayTest, ClearResetsFields) {
    obj.fixed_ints[0] = 42;
    obj.fixed_longs[0] = 999L;
    obj.fixed_strings[0] = sstr("hello");
    FixedArrayStruct_clear(&obj);
    EXPECT_EQ(obj.fixed_ints[0], 0);
    EXPECT_EQ(obj.fixed_longs[0], 0);
    EXPECT_EQ(obj.fixed_strings[0], nullptr);
}

// =============================================================================
// Marshal - Int array
// =============================================================================

TEST_F(FixedArrayTest, MarshalIntArray) {
    obj.fixed_ints[0] = 1;
    obj.fixed_ints[1] = 2;
    obj.fixed_ints[2] = 3;
    obj.fixed_ints[3] = 4;
    obj.fixed_ints[4] = 5;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);
    std::string json(sstr_cstr(out), sstr_length(out));
    sstr_free(out);

    EXPECT_NE(json.find("\"fixed_ints\":[1,2,3,4,5]"), std::string::npos);
}

TEST_F(FixedArrayTest, MarshalZeroInitIntArray) {
    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);
    std::string json(sstr_cstr(out), sstr_length(out));
    sstr_free(out);

    EXPECT_NE(json.find("\"fixed_ints\":[0,0,0,0,0]"), std::string::npos);
}

// =============================================================================
// Round-trip: Int
// =============================================================================

TEST_F(FixedArrayTest, RoundTripIntArray) {
    obj.fixed_ints[0] = -10;
    obj.fixed_ints[1] = 0;
    obj.fixed_ints[2] = 100;
    obj.fixed_ints[3] = 2147483647;
    obj.fixed_ints[4] = -2147483647;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(obj.fixed_ints[i], obj2.fixed_ints[i]);
    }

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Long
// =============================================================================

TEST_F(FixedArrayTest, RoundTripLongArray) {
    obj.fixed_longs[0] = 100000L;
    obj.fixed_longs[1] = -200000L;
    obj.fixed_longs[2] = 0L;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(obj.fixed_longs[i], obj2.fixed_longs[i]);
    }

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Float
// =============================================================================

TEST_F(FixedArrayTest, RoundTripFloatArray) {
    obj.fixed_floats[0] = 1.5f;
    obj.fixed_floats[1] = -2.75f;
    obj.fixed_floats[2] = 0.0f;
    obj.fixed_floats[3] = 3.14f;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(obj.fixed_floats[i], obj2.fixed_floats[i], 0.01f);
    }

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Double
// =============================================================================

TEST_F(FixedArrayTest, RoundTripDoubleArray) {
    obj.fixed_doubles[0] = 1.123456789;
    obj.fixed_doubles[1] = -9.87654321;
    obj.fixed_doubles[2] = 0.0;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(obj.fixed_doubles[i], obj2.fixed_doubles[i], 0.0001);
    }

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: String (sstr_t)
// =============================================================================

TEST_F(FixedArrayTest, RoundTripStringArray) {
    obj.fixed_strings[0] = sstr("hello");
    obj.fixed_strings[1] = sstr("world");
    obj.fixed_strings[2] = sstr("");

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    EXPECT_STREQ(sstr_cstr(obj2.fixed_strings[0]), "hello");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_strings[1]), "world");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_strings[2]), "");

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Bool
// =============================================================================

TEST_F(FixedArrayTest, RoundTripBoolArray) {
    obj.fixed_bools[0] = 1;
    obj.fixed_bools[1] = 0;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj2.fixed_bools[0], 1);
    EXPECT_EQ(obj2.fixed_bools[1], 0);

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Enum
// =============================================================================

TEST_F(FixedArrayTest, RoundTripEnumArray) {
    obj.fixed_colors[0] = Color_RED;
    obj.fixed_colors[1] = Color_GREEN;
    obj.fixed_colors[2] = Color_BLUE;

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);
    std::string json(sstr_cstr(out), sstr_length(out));

    // enum arrays should marshal as strings
    EXPECT_NE(json.find("\"RED\""), std::string::npos);
    EXPECT_NE(json.find("\"GREEN\""), std::string::npos);
    EXPECT_NE(json.find("\"BLUE\""), std::string::npos);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj2.fixed_colors[0], Color_RED);
    EXPECT_EQ(obj2.fixed_colors[1], Color_GREEN);
    EXPECT_EQ(obj2.fixed_colors[2], Color_BLUE);

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Round-trip: Nested struct
// =============================================================================

TEST_F(FixedArrayTest, RoundTripStructArray) {
    obj.fixed_contacts[0].name = sstr("Alice");
    obj.fixed_contacts[0].age = sstr("30");
    obj.fixed_contacts[1].name = sstr("Bob");
    obj.fixed_contacts[1].age = sstr("25");

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[0].name), "Alice");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[0].age), "30");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[1].name), "Bob");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[1].age), "25");

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}

// =============================================================================
// Partial fill (fewer elements than array size)
// =============================================================================

TEST_F(FixedArrayTest, UnmarshalPartialIntArray) {
    sstr_t json = sstr(
        "{\"fixed_ints\":[10,20],"
        "\"fixed_longs\":[0,0,0],"
        "\"fixed_floats\":[0,0,0,0],"
        "\"fixed_doubles\":[0,0,0],"
        "\"fixed_strings\":[\"\",\"\",\"\"],"
        "\"fixed_bools\":[false,false],"
        "\"fixed_colors\":[\"RED\",\"RED\",\"RED\"],"
        "\"fixed_contacts\":[{\"name\":\"\",\"age\":\"\"},{\"name\":\"\",\"age\":\"\"}]}"
    );
    int r = json_unmarshal_FixedArrayStruct(json, &obj);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj.fixed_ints[0], 10);
    EXPECT_EQ(obj.fixed_ints[1], 20);
    // remaining elements stay zero-initialized
    EXPECT_EQ(obj.fixed_ints[2], 0);
    EXPECT_EQ(obj.fixed_ints[3], 0);
    EXPECT_EQ(obj.fixed_ints[4], 0);

    sstr_free(json);
}

// =============================================================================
// Empty JSON array for fixed-size array
// =============================================================================

TEST_F(FixedArrayTest, UnmarshalEmptyArray) {
    sstr_t json = sstr(
        "{\"fixed_ints\":[],"
        "\"fixed_longs\":[0,0,0],"
        "\"fixed_floats\":[0,0,0,0],"
        "\"fixed_doubles\":[0,0,0],"
        "\"fixed_strings\":[\"\",\"\",\"\"],"
        "\"fixed_bools\":[false,false],"
        "\"fixed_colors\":[\"RED\",\"RED\",\"RED\"],"
        "\"fixed_contacts\":[{\"name\":\"\",\"age\":\"\"},{\"name\":\"\",\"age\":\"\"}]}"
    );
    int r = json_unmarshal_FixedArrayStruct(json, &obj);
    EXPECT_EQ(r, 0);

    // all should be zero
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(obj.fixed_ints[i], 0);
    }

    sstr_free(json);
}

// =============================================================================
// Overflow: more elements than array size → should error
// =============================================================================

TEST_F(FixedArrayTest, UnmarshalOverflowErrors) {
    sstr_t json = sstr(
        "{\"fixed_ints\":[1,2,3,4,5,6],"
        "\"fixed_longs\":[0,0,0],"
        "\"fixed_floats\":[0,0,0,0],"
        "\"fixed_doubles\":[0,0,0],"
        "\"fixed_strings\":[\"\",\"\",\"\"],"
        "\"fixed_bools\":[false,false],"
        "\"fixed_colors\":[\"RED\",\"RED\",\"RED\"],"
        "\"fixed_contacts\":[{\"name\":\"\",\"age\":\"\"},{\"name\":\"\",\"age\":\"\"}]}"
    );
    int r = json_unmarshal_FixedArrayStruct(json, &obj);
    EXPECT_NE(r, 0);  // should fail

    sstr_free(json);
}

// =============================================================================
// Struct layout: no _len fields for fixed arrays
// =============================================================================

TEST_F(FixedArrayTest, StructSizeHasNoLenFields) {
    // fixed_ints is int[5], not int* + int
    EXPECT_EQ(sizeof(obj.fixed_ints), 5 * sizeof(int));
    EXPECT_EQ(sizeof(obj.fixed_longs), 3 * sizeof(long));
    EXPECT_EQ(sizeof(obj.fixed_floats), 4 * sizeof(float));
    EXPECT_EQ(sizeof(obj.fixed_doubles), 3 * sizeof(double));
    EXPECT_EQ(sizeof(obj.fixed_strings), 3 * sizeof(sstr_t));
    EXPECT_EQ(sizeof(obj.fixed_bools), 2 * sizeof(int));
    EXPECT_EQ(sizeof(obj.fixed_colors), 3 * sizeof(int));
    EXPECT_EQ(sizeof(obj.fixed_contacts), 2 * sizeof(struct Person));
}

// =============================================================================
// Full round-trip with all fields populated
// =============================================================================

TEST_F(FixedArrayTest, FullRoundTrip) {
    obj.fixed_ints[0] = 1; obj.fixed_ints[1] = 2; obj.fixed_ints[2] = 3;
    obj.fixed_ints[3] = 4; obj.fixed_ints[4] = 5;
    obj.fixed_longs[0] = 100L; obj.fixed_longs[1] = 200L; obj.fixed_longs[2] = 300L;
    obj.fixed_floats[0] = 1.1f; obj.fixed_floats[1] = 2.2f;
    obj.fixed_floats[2] = 3.3f; obj.fixed_floats[3] = 4.4f;
    obj.fixed_doubles[0] = 1.11; obj.fixed_doubles[1] = 2.22; obj.fixed_doubles[2] = 3.33;
    obj.fixed_strings[0] = sstr("a"); obj.fixed_strings[1] = sstr("b"); obj.fixed_strings[2] = sstr("c");
    obj.fixed_bools[0] = 1; obj.fixed_bools[1] = 0;
    obj.fixed_colors[0] = Color_RED; obj.fixed_colors[1] = Color_GREEN; obj.fixed_colors[2] = Color_BLUE;
    obj.fixed_contacts[0].name = sstr("X"); obj.fixed_contacts[0].age = sstr("1");
    obj.fixed_contacts[1].name = sstr("Y"); obj.fixed_contacts[1].age = sstr("2");

    sstr_t out = sstr_new();
    json_marshal_FixedArrayStruct(&obj, out);

    struct FixedArrayStruct obj2;
    FixedArrayStruct_init(&obj2);
    int r = json_unmarshal_FixedArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);

    for (int i = 0; i < 5; i++) EXPECT_EQ(obj.fixed_ints[i], obj2.fixed_ints[i]);
    for (int i = 0; i < 3; i++) EXPECT_EQ(obj.fixed_longs[i], obj2.fixed_longs[i]);
    for (int i = 0; i < 4; i++) EXPECT_NEAR(obj.fixed_floats[i], obj2.fixed_floats[i], 0.01f);
    for (int i = 0; i < 3; i++) EXPECT_NEAR(obj.fixed_doubles[i], obj2.fixed_doubles[i], 0.001);
    for (int i = 0; i < 3; i++) EXPECT_STREQ(sstr_cstr(obj.fixed_strings[i]), sstr_cstr(obj2.fixed_strings[i]));
    for (int i = 0; i < 2; i++) EXPECT_EQ(obj.fixed_bools[i], obj2.fixed_bools[i]);
    for (int i = 0; i < 3; i++) EXPECT_EQ(obj.fixed_colors[i], obj2.fixed_colors[i]);
    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[0].name), "X");
    EXPECT_STREQ(sstr_cstr(obj2.fixed_contacts[1].name), "Y");

    sstr_free(out);
    FixedArrayStruct_clear(&obj2);
}
