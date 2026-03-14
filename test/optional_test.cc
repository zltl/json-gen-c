/**
 * @file optional_test.cc
 * @brief Tests for optional and nullable field support
 */

#include <gtest/gtest.h>
#include <string.h>

#include "json.gen.h"
#include "sstr.h"

// ============================================================
// OptionalOnlyStruct tests
// ============================================================

TEST(OptionalTest, OptionalFieldsDefaultInit) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_score);
    EXPECT_FALSE(obj.has_active);
    EXPECT_FALSE(obj.has_rating);
    EXPECT_FALSE(obj.has_precise);
    EXPECT_FALSE(obj.has_big_num);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, MarshalAllFieldsPresent) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 42;
    obj.name = sstr("Alice");
    obj.has_name = true;
    obj.score = 100;
    obj.has_score = true;
    obj.active = 1;
    obj.has_active = true;
    obj.rating = 4.5f;
    obj.has_rating = true;
    obj.precise = 3.14159;
    obj.has_precise = true;
    obj.big_num = 9999999999L;
    obj.has_big_num = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalOnlyStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"id\":42"), nullptr);
    EXPECT_NE(strstr(s, "\"name\":\"Alice\""), nullptr);
    EXPECT_NE(strstr(s, "\"score\":100"), nullptr);
    EXPECT_NE(strstr(s, "\"active\":true"), nullptr);
    EXPECT_NE(strstr(s, "\"rating\":"), nullptr);
    EXPECT_NE(strstr(s, "\"precise\":"), nullptr);
    EXPECT_NE(strstr(s, "\"big_num\":9999999999"), nullptr);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, MarshalSomeFieldsAbsent) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 1;
    obj.name = sstr("Bob");
    obj.has_name = true;
    // score, active, rating, precise, big_num all absent

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalOnlyStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"id\":1"), nullptr);
    EXPECT_NE(strstr(s, "\"name\":\"Bob\""), nullptr);
    // These should NOT appear
    EXPECT_EQ(strstr(s, "\"score\""), nullptr);
    EXPECT_EQ(strstr(s, "\"active\""), nullptr);
    EXPECT_EQ(strstr(s, "\"rating\""), nullptr);
    EXPECT_EQ(strstr(s, "\"precise\""), nullptr);
    EXPECT_EQ(strstr(s, "\"big_num\""), nullptr);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, MarshalOnlyRequired) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 99;
    // All optional fields absent

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalOnlyStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"id\":99"), nullptr);
    // No optional fields should be present
    EXPECT_EQ(strstr(s, "\"name\""), nullptr);
    EXPECT_EQ(strstr(s, "\"score\""), nullptr);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, UnmarshalAllFieldsPresent) {
    sstr_t json = sstr("{\"id\":42,\"name\":\"Alice\",\"score\":100,"
                       "\"active\":true,\"rating\":4.5,\"precise\":3.14,"
                       "\"big_num\":9999999999}");
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_OptionalOnlyStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 42);
    EXPECT_TRUE(obj.has_name);
    EXPECT_STREQ(sstr_cstr(obj.name), "Alice");
    EXPECT_TRUE(obj.has_score);
    EXPECT_EQ(obj.score, 100);
    EXPECT_TRUE(obj.has_active);
    EXPECT_EQ(obj.active, 1);
    EXPECT_TRUE(obj.has_rating);
    EXPECT_FLOAT_EQ(obj.rating, 4.5f);
    EXPECT_TRUE(obj.has_precise);
    EXPECT_NEAR(obj.precise, 3.14, 0.001);
    EXPECT_TRUE(obj.has_big_num);
    EXPECT_EQ(obj.big_num, 9999999999L);

    sstr_free(json);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, UnmarshalMissingOptionalFields) {
    sstr_t json = sstr("{\"id\":7}");
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_OptionalOnlyStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 7);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_score);
    EXPECT_FALSE(obj.has_active);
    EXPECT_FALSE(obj.has_rating);
    EXPECT_FALSE(obj.has_precise);
    EXPECT_FALSE(obj.has_big_num);

    sstr_free(json);
    OptionalOnlyStruct_clear(&obj);
}

TEST(OptionalTest, RoundTripOptionalPresent) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 10;
    obj.name = sstr("test");
    obj.has_name = true;
    obj.score = 50;
    obj.has_score = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalOnlyStruct(&obj, out), 0);

    struct OptionalOnlyStruct obj2;
    OptionalOnlyStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_OptionalOnlyStruct(out, &obj2), 0);

    EXPECT_EQ(obj2.id, 10);
    EXPECT_TRUE(obj2.has_name);
    EXPECT_STREQ(sstr_cstr(obj2.name), "test");
    EXPECT_TRUE(obj2.has_score);
    EXPECT_EQ(obj2.score, 50);
    EXPECT_FALSE(obj2.has_active);
    EXPECT_FALSE(obj2.has_rating);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
    OptionalOnlyStruct_clear(&obj2);
}

TEST(OptionalTest, RoundTripOptionalAbsent) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 20;
    // All optional absent

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalOnlyStruct(&obj, out), 0);

    struct OptionalOnlyStruct obj2;
    OptionalOnlyStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_OptionalOnlyStruct(out, &obj2), 0);

    EXPECT_EQ(obj2.id, 20);
    EXPECT_FALSE(obj2.has_name);
    EXPECT_FALSE(obj2.has_score);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
    OptionalOnlyStruct_clear(&obj2);
}

// ============================================================
// NullableOnlyStruct tests
// ============================================================

TEST(NullableTest, MarshalNullValues) {
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    obj.id = 1;
    // has_name, has_score, has_active all false → emit null

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_NullableOnlyStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"id\":1"), nullptr);
    EXPECT_NE(strstr(s, "\"name\":null"), nullptr);
    EXPECT_NE(strstr(s, "\"score\":null"), nullptr);
    EXPECT_NE(strstr(s, "\"active\":null"), nullptr);

    sstr_free(out);
    NullableOnlyStruct_clear(&obj);
}

TEST(NullableTest, MarshalWithValues) {
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    obj.id = 2;
    obj.name = sstr("Charlie");
    obj.has_name = true;
    obj.score = 88;
    obj.has_score = true;
    obj.active = 1;
    obj.has_active = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_NullableOnlyStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"name\":\"Charlie\""), nullptr);
    EXPECT_NE(strstr(s, "\"score\":88"), nullptr);
    EXPECT_NE(strstr(s, "\"active\":true"), nullptr);
    EXPECT_EQ(strstr(s, "null"), nullptr);

    sstr_free(out);
    NullableOnlyStruct_clear(&obj);
}

TEST(NullableTest, UnmarshalNullValues) {
    sstr_t json = sstr("{\"id\":5,\"name\":null,\"score\":null,\"active\":null}");
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_NullableOnlyStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 5);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_score);
    EXPECT_FALSE(obj.has_active);

    sstr_free(json);
    NullableOnlyStruct_clear(&obj);
}

TEST(NullableTest, UnmarshalMixedValues) {
    sstr_t json = sstr("{\"id\":3,\"name\":\"Dave\",\"score\":null,\"active\":true}");
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_NullableOnlyStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 3);
    EXPECT_TRUE(obj.has_name);
    EXPECT_STREQ(sstr_cstr(obj.name), "Dave");
    EXPECT_FALSE(obj.has_score);
    EXPECT_TRUE(obj.has_active);
    EXPECT_EQ(obj.active, 1);

    sstr_free(json);
    NullableOnlyStruct_clear(&obj);
}

TEST(NullableTest, RoundTripNullable) {
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    obj.id = 10;
    obj.name = sstr("test");
    obj.has_name = true;
    // score null, active null

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_NullableOnlyStruct(&obj, out), 0);

    struct NullableOnlyStruct obj2;
    NullableOnlyStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_NullableOnlyStruct(out, &obj2), 0);

    EXPECT_EQ(obj2.id, 10);
    EXPECT_TRUE(obj2.has_name);
    EXPECT_STREQ(sstr_cstr(obj2.name), "test");
    EXPECT_FALSE(obj2.has_score);
    EXPECT_FALSE(obj2.has_active);

    sstr_free(out);
    NullableOnlyStruct_clear(&obj);
    NullableOnlyStruct_clear(&obj2);
}

// ============================================================
// OptionalNullableStruct tests (combined modifiers)
// ============================================================

TEST(OptionalNullableTest, MarshalAbsent) {
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    obj.id = 1;
    // name and score both absent → skip entirely (optional dominates)

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalNullableStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"id\":1"), nullptr);
    EXPECT_EQ(strstr(s, "\"name\""), nullptr);
    EXPECT_EQ(strstr(s, "\"score\""), nullptr);

    sstr_free(out);
    OptionalNullableStruct_clear(&obj);
}

TEST(OptionalNullableTest, MarshalPresent) {
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    obj.id = 2;
    obj.name = sstr("Eve");
    obj.has_name = true;
    obj.score = 77;
    obj.has_score = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalNullableStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"name\":\"Eve\""), nullptr);
    EXPECT_NE(strstr(s, "\"score\":77"), nullptr);

    sstr_free(out);
    OptionalNullableStruct_clear(&obj);
}

TEST(OptionalNullableTest, UnmarshalNull) {
    sstr_t json = sstr("{\"id\":3,\"name\":null,\"score\":null}");
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_OptionalNullableStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 3);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_score);

    sstr_free(json);
    OptionalNullableStruct_clear(&obj);
}

TEST(OptionalNullableTest, UnmarshalMissing) {
    sstr_t json = sstr("{\"id\":4}");
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_OptionalNullableStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 4);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_score);

    sstr_free(json);
    OptionalNullableStruct_clear(&obj);
}

TEST(OptionalNullableTest, UnmarshalValues) {
    sstr_t json = sstr("{\"id\":5,\"name\":\"Frank\",\"score\":90}");
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_OptionalNullableStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 5);
    EXPECT_TRUE(obj.has_name);
    EXPECT_STREQ(sstr_cstr(obj.name), "Frank");
    EXPECT_TRUE(obj.has_score);
    EXPECT_EQ(obj.score, 90);

    sstr_free(json);
    OptionalNullableStruct_clear(&obj);
}

TEST(OptionalNullableTest, RoundTrip) {
    struct OptionalNullableStruct obj;
    OptionalNullableStruct_init(&obj);
    obj.id = 1;
    obj.name = sstr("roundtrip");
    obj.has_name = true;
    // score absent

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_OptionalNullableStruct(&obj, out), 0);

    struct OptionalNullableStruct obj2;
    OptionalNullableStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_OptionalNullableStruct(out, &obj2), 0);

    EXPECT_EQ(obj2.id, 1);
    EXPECT_TRUE(obj2.has_name);
    EXPECT_STREQ(sstr_cstr(obj2.name), "roundtrip");
    EXPECT_FALSE(obj2.has_score);

    sstr_free(out);
    OptionalNullableStruct_clear(&obj);
    OptionalNullableStruct_clear(&obj2);
}

// ============================================================
// NullableNestedStruct tests (nullable struct/enum)
// ============================================================

TEST(NullableNestedTest, MarshalNullStruct) {
    struct NullableNestedStruct obj;
    NullableNestedStruct_init(&obj);
    obj.id = 1;
    // has_person = false → emit "person":null
    // has_color = false → skip (optional only)

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_NullableNestedStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"person\":null"), nullptr);
    EXPECT_EQ(strstr(s, "\"color\""), nullptr);
    EXPECT_EQ(strstr(s, "\"status\""), nullptr);

    sstr_free(out);
    NullableNestedStruct_clear(&obj);
}

TEST(NullableNestedTest, MarshalWithValues) {
    struct NullableNestedStruct obj;
    NullableNestedStruct_init(&obj);
    obj.id = 2;
    Person_init(&obj.person);
    obj.person.name = sstr("Grace");
    obj.person.age = sstr("30");
    obj.has_person = true;
    obj.color = Color_RED;
    obj.has_color = true;
    obj.status = Status_ACTIVE;
    obj.has_status = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_NullableNestedStruct(&obj, out), 0);

    const char* s = sstr_cstr(out);
    EXPECT_NE(strstr(s, "\"person\":{"), nullptr);
    EXPECT_NE(strstr(s, "\"Grace\""), nullptr);
    EXPECT_NE(strstr(s, "\"color\":\"RED\""), nullptr);
    EXPECT_NE(strstr(s, "\"status\":\"ACTIVE\""), nullptr);

    sstr_free(out);
    NullableNestedStruct_clear(&obj);
}

TEST(NullableNestedTest, UnmarshalNullPerson) {
    sstr_t json = sstr("{\"id\":3,\"person\":null}");
    struct NullableNestedStruct obj;
    NullableNestedStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_NullableNestedStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 3);
    EXPECT_FALSE(obj.has_person);

    sstr_free(json);
    NullableNestedStruct_clear(&obj);
}

TEST(NullableNestedTest, UnmarshalOptionalEnum) {
    sstr_t json = sstr("{\"id\":4,\"person\":null,\"color\":\"BLUE\"}");
    struct NullableNestedStruct obj;
    NullableNestedStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_NullableNestedStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 4);
    EXPECT_FALSE(obj.has_person);
    EXPECT_TRUE(obj.has_color);
    EXPECT_EQ(obj.color, Color_BLUE);

    sstr_free(json);
    NullableNestedStruct_clear(&obj);
}

TEST(NullableNestedTest, UnmarshalOptionalNullableStatus) {
    // status is optional nullable — accepts null
    sstr_t json = sstr("{\"id\":5,\"person\":null,\"status\":null}");
    struct NullableNestedStruct obj;
    NullableNestedStruct_init(&obj);
    ASSERT_EQ(json_unmarshal_NullableNestedStruct(json, &obj), 0);

    EXPECT_EQ(obj.id, 5);
    EXPECT_FALSE(obj.has_person);
    EXPECT_FALSE(obj.has_status);

    sstr_free(json);
    NullableNestedStruct_clear(&obj);
}

// ============================================================
// Pretty-print round trip with indent
// ============================================================

TEST(OptionalTest, PrettyPrintRoundTrip) {
    struct OptionalOnlyStruct obj;
    OptionalOnlyStruct_init(&obj);
    obj.id = 42;
    obj.name = sstr("pretty");
    obj.has_name = true;

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_indent_OptionalOnlyStruct(&obj, 2, 0, out), 0);

    // Verify it contains newlines (pretty printed)
    EXPECT_NE(strstr(sstr_cstr(out), "\n"), nullptr);

    struct OptionalOnlyStruct obj2;
    OptionalOnlyStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_OptionalOnlyStruct(out, &obj2), 0);

    EXPECT_EQ(obj2.id, 42);
    EXPECT_TRUE(obj2.has_name);
    EXPECT_STREQ(sstr_cstr(obj2.name), "pretty");
    EXPECT_FALSE(obj2.has_score);

    sstr_free(out);
    OptionalOnlyStruct_clear(&obj);
    OptionalOnlyStruct_clear(&obj2);
}
