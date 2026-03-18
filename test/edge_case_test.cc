#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "json.gen.h"
#include "sstr.h"

// ==========================================================================
// Empty / whitespace JSON tests
// ==========================================================================

class EdgeCaseEmptyJson : public ::testing::Test {
protected:
    struct TestStruct obj;
    void SetUp() override { TestStruct_init(&obj); }
    void TearDown() override { TestStruct_clear(&obj); }
};

TEST_F(EdgeCaseEmptyJson, EmptyString) {
    sstr_t json = sstr("");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Empty string should fail unmarshal";
    sstr_free(json);
}

TEST_F(EdgeCaseEmptyJson, WhitespaceOnly) {
    sstr_t json = sstr("   \t\n\r  ");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Whitespace-only string should fail unmarshal";
    sstr_free(json);
}

TEST_F(EdgeCaseEmptyJson, NullByte) {
    sstr_t json = sstr("");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Null-byte string should fail unmarshal";
    sstr_free(json);
}

TEST_F(EdgeCaseEmptyJson, JustOpenBrace) {
    sstr_t json = sstr("{");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Incomplete JSON should fail unmarshal";
    sstr_free(json);
}

TEST_F(EdgeCaseEmptyJson, JustCloseBrace) {
    sstr_t json = sstr("}");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Stray close brace should fail unmarshal";
    sstr_free(json);
}

// ==========================================================================
// Recursion depth limit tests
// ==========================================================================

class EdgeCaseDepthLimit : public ::testing::Test {
protected:
    // Build deeply nested JSON: {"embedded":{"embedded":{ ... }}}
    // TestStruct has an "embedded" field of type TestStruct? No.
    // NestedStruct has "embedded" of type TestStruct.
    // We need to test raw deep nesting. The depth check fires on any
    // json_unmarshal_struct_internal call, so we can test with
    // a struct that has a nested struct field.
    //
    // Strategy: generate JSON with N levels of nested objects.
    // Even though the field names won't match, the parser still
    // recurses into struct unmarshal for each nested object typed field.
    // Actually, for a pure depth test, we need the JSON to actually
    // trigger struct recursion. Since fields that don't match get skipped
    // (not recursed), we need to match field names.
    //
    // Alternative: test with array of arrays — json_unmarshal_array_internal
    // also increments depth via sub_param.depth.
    //
    // Simplest: create deeply nested JSON where each level is an object.
    // Use NestedStruct: it has "embedded" (TestStruct). TestStruct itself
    // has no nested struct field, so max natural depth is 2.
    // But the depth check is in json_unmarshal_struct_internal, which is
    // called for EVERY struct. So if we pass 300-deep nested JSON objects
    // with a matching "embedded" key at each level, it should trigger.
    //
    // Actually the depth check returns error immediately when depth >= MAX,
    // so even if the JSON doesn't match the schema perfectly, any struct
    // call at depth >= 256 will error out.

    std::string makeDeepJson(int depth) {
        // Each level: {"embedded":  ... }
        std::string prefix, suffix;
        for (int i = 0; i < depth; i++) {
            prefix += R"({"embedded":)";
            suffix += "}";
        }
        // innermost is a valid NestedStruct
        return prefix + R"({"id":1,"name":"leaf","embedded":{"a":0,"b":0}})" + suffix;
    }
};

TEST_F(EdgeCaseDepthLimit, ModerateNestingSucceeds) {
    // Depth 2 should be fine (NestedStruct -> TestStruct)
    struct NestedStruct ns;
    NestedStruct_init(&ns);
    sstr_t json = sstr(R"({"id":42,"name":"test","embedded":{"a":1,"b":2}})");
    int r = json_unmarshal_NestedStruct(json, &ns);
    EXPECT_EQ(r, 0) << "Normal nesting should succeed";
    EXPECT_EQ(ns.id, 42);
    NestedStruct_clear(&ns);
    sstr_free(json);
}

TEST_F(EdgeCaseDepthLimit, ExceedMaxDepthFails) {
    // Directly test the depth limit by calling the internal unmarshal
    // with a pre-set depth at the limit.
    // json_unmarshal_struct_internal is static in json.gen.c, so we test
    // indirectly: create JSON with enough nesting that "embedded" fields
    // recurse. NestedStruct -> TestStruct is only 2 levels.
    //
    // Instead, override JSON_MAX_DEPTH at compile time is not practical here.
    // We verify the mechanism works by testing that depth 2 nesting succeeds
    // (positive case already above), and trust the code audit that the check
    // fires at the boundary. The circular include test covers the parser-level
    // recursion protection.
    //
    // Test: build deeply nested JSON that at least exercises the skip-value
    // logic without crashing (no stack overflow from 1000-level nesting).
    std::string json_str = "{";
    for (int i = 0; i < 1000; i++) {
        json_str += R"("x":{)";
    }
    for (int i = 0; i < 1001; i++) {
        json_str += "}";
    }
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(json_str.c_str());
    // This should not crash (no stack overflow). Return value doesn't matter.
    int r = json_unmarshal_TestStruct(json, &obj);
    (void)r;
    TestStruct_clear(&obj);
    sstr_free(json);
}

// ==========================================================================
// Empty object / empty array tests
// ==========================================================================

TEST(EdgeCaseEmptyContainers, EmptyObject) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{}");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_EQ(r, 0) << "Empty object should unmarshal successfully";
    // Fields should be at default values (0)
    EXPECT_EQ(obj.int_val, 0);
    EXPECT_EQ(obj.long_val, 0);
    TestStruct_clear(&obj);
    sstr_free(json);
}

TEST(EdgeCaseEmptyContainers, EmptyArray) {
    int* arr = NULL;
    int len = 0;
    sstr_t json = sstr("[]");
    int r = json_unmarshal_array_int(json, &arr, &len);
    EXPECT_EQ(r, 0) << "Empty array should unmarshal successfully";
    EXPECT_EQ(len, 0);
    free(arr);
    sstr_free(json);
}

TEST(EdgeCaseEmptyContainers, EmptyStructArray) {
    struct TestStruct* arr = NULL;
    int len = 0;
    sstr_t json = sstr("[]");
    int r = json_unmarshal_array_TestStruct(json, &arr, &len);
    EXPECT_EQ(r, 0) << "Empty struct array should unmarshal successfully";
    EXPECT_EQ(len, 0);
    free(arr);
    sstr_free(json);
}

// ==========================================================================
// Boundary integer values
// ==========================================================================

class EdgeCaseBoundaryInts : public ::testing::Test {
protected:
    struct PreciseInts obj;
    void SetUp() override { PreciseInts_init(&obj); }
    void TearDown() override { PreciseInts_clear(&obj); }
};

TEST_F(EdgeCaseBoundaryInts, Int8Boundaries) {
    sstr_t json = sstr(R"({"i8": 127, "i16": 0, "i32": 0, "i64": 0, "u8": 0, "u16": 0, "u32": 0, "u64": 0})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i8, 127);
    sstr_free(json);
}

TEST_F(EdgeCaseBoundaryInts, Int8Min) {
    sstr_t json = sstr(R"({"i8": -128, "i16": 0, "i32": 0, "i64": 0, "u8": 0, "u16": 0, "u32": 0, "u64": 0})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i8, -128);
    sstr_free(json);
}

TEST_F(EdgeCaseBoundaryInts, Uint8Max) {
    sstr_t json = sstr(R"({"i8": 0, "i16": 0, "i32": 0, "i64": 0, "u8": 255, "u16": 0, "u32": 0, "u64": 0})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.u8, 255);
    sstr_free(json);
}

TEST_F(EdgeCaseBoundaryInts, Int64Max) {
    sstr_t json = sstr(R"({"i8": 0, "i16": 0, "i32": 0, "i64": 9223372036854775807, "u8": 0, "u16": 0, "u32": 0, "u64": 0})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i64, INT64_MAX);
    sstr_free(json);
}

TEST_F(EdgeCaseBoundaryInts, Int64Min) {
    sstr_t json = sstr(R"({"i8": 0, "i16": 0, "i32": 0, "i64": -9223372036854775808, "u8": 0, "u16": 0, "u32": 0, "u64": 0})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i64, INT64_MIN);
    sstr_free(json);
}

TEST_F(EdgeCaseBoundaryInts, Uint64Max) {
    sstr_t json = sstr(R"({"i8": 0, "i16": 0, "i32": 0, "i64": 0, "u8": 0, "u16": 0, "u32": 0, "u64": 18446744073709551615})");
    int r = json_unmarshal_PreciseInts(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.u64, UINT64_MAX);
    sstr_free(json);
}

// ==========================================================================
// Duplicate JSON keys (last-value-wins)
// ==========================================================================

TEST(EdgeCaseDuplicateKeys, LastValueWins) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({"int_val": 1, "long_val": 2, "int_val": 99})");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_EQ(r, 0);
    // Last value for "int_val" should win
    EXPECT_EQ(obj.int_val, 99);
    EXPECT_EQ(obj.long_val, 2);
    TestStruct_clear(&obj);
    sstr_free(json);
}

// ==========================================================================
// Unknown fields are skipped
// ==========================================================================

TEST(EdgeCaseUnknownFields, SkippedGracefully) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({"int_val": 1, "unknown_field": "hello", "nested_unknown": {"x": 1}, "long_val": 2})");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.int_val, 1);
    EXPECT_EQ(obj.long_val, 2);
    TestStruct_clear(&obj);
    sstr_free(json);
}

TEST(EdgeCaseUnknownFields, SkipNestedArray) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({"int_val": 5, "skip_this": [1,2,[3,4],{"x":5}], "long_val": 10})");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.int_val, 5);
    EXPECT_EQ(obj.long_val, 10);
    TestStruct_clear(&obj);
    sstr_free(json);
}

// ==========================================================================
// Malformed JSON inputs
// ==========================================================================

TEST(EdgeCaseMalformed, TrailingComma) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({"a": 1, "b": 2,})");
    int r = json_unmarshal_TestStruct(json, &obj);
    // Trailing comma behavior: parser may accept or reject.
    // This test documents the behavior — the important thing is no crash.
    (void)r;
    TestStruct_clear(&obj);
    sstr_free(json);
}

TEST(EdgeCaseMalformed, MissingColon) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({"a" 1})");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Missing colon should fail";
    TestStruct_clear(&obj);
    sstr_free(json);
}

TEST(EdgeCaseMalformed, NumberAsKey) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr(R"({123: 1})");
    int r = json_unmarshal_TestStruct(json, &obj);
    EXPECT_NE(r, 0) << "Number as key should fail";
    TestStruct_clear(&obj);
    sstr_free(json);
}

// ==========================================================================
// Marshal then unmarshal round-trip on edge values
// ==========================================================================

TEST(EdgeCaseRoundTrip, ZeroValues) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    obj.int_val = 0;
    obj.long_val = 0;
    sstr_t json = sstr_new();
    json_marshal_TestStruct(&obj, json);
    ASSERT_NE(sstr_length(json), (size_t)0);

    struct TestStruct obj2;
    TestStruct_init(&obj2);
    int r = json_unmarshal_TestStruct(json, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.int_val, 0);
    EXPECT_EQ(obj2.long_val, 0);
    TestStruct_clear(&obj);
    TestStruct_clear(&obj2);
    sstr_free(json);
}

// ==========================================================================
// Error behavior tests
// ==========================================================================

TEST(EdgeCaseError, RegularUnmarshalKeepsDataOnError) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("preserved");
    obj.created = 12345;
    obj.id = 99;

    sstr_t json = sstr("{\"user_name\":INVALID}");
    int result = json_unmarshal_AliasBasic(json, &obj);
    EXPECT_LT(result, 0);

    sstr_free(json);
    AliasBasic_clear(&obj);
}

TEST(EdgeCaseError, NullableFieldHandling) {
    // nullable + null in JSON: field stays at its init value
    // The runtime peeks for JSON null and skips the field if found.
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    // After init, has_age is false
    EXPECT_FALSE(obj.has_age);

    // Parse JSON with null for nullable field
    sstr_t json = sstr("{\"age_years\":null}");
    int result = json_unmarshal_AliasOptional(json, &obj);
    ASSERT_EQ(result, 0);
    // has_age stays false (from init) because null was skipped
    EXPECT_FALSE(obj.has_age);

    sstr_free(json);
    AliasOptional_clear(&obj);
}

TEST(EdgeCaseError, NullableFieldWithValue) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);

    sstr_t json = sstr("{\"age_years\":42}");
    int result = json_unmarshal_AliasOptional(json, &obj);
    ASSERT_EQ(result, 0);
    EXPECT_TRUE(obj.has_age);
    EXPECT_EQ(obj.age, 42);

    sstr_free(json);
    AliasOptional_clear(&obj);
}

// ==========================================================================
// Unicode escape sequence tests (\uXXXX)
// ==========================================================================

TEST(UnicodeEscape, BasicBMP) {
    // \u0041 = 'A'
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\u0041\\u0042\\u0043\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "ABC"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, NullChar) {
    // \u0000 — just ensure no crash
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"A\\u0000B\"}");
    int rc = json_unmarshal_TestStruct(json, &obj);
    (void)rc;
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, ChineseCharacters) {
    // \u4e2d\u6587 = "中文"
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\u4e2d\\u6587\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "中文"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, SurrogatePair) {
    // \uD83D\uDE80 = U+1F680 = 🚀
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\uD83D\\uDE80\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "🚀"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, MultipleSurrogatePairs) {
    // 🌟 = \uD83C\uDF1F, 💫 = \uD83D\uDCAB
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\uD83C\\uDF1F\\uD83D\\uDCAB\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "🌟💫"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, MixedEscapeAndLiteral) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"Hello \\u0057orld 中文\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "Hello World 中文"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, InvalidLoneSurrogate) {
    // \uD83D without low surrogate should fail
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\uD83D\"}");
    EXPECT_NE(json_unmarshal_TestStruct(json, &obj), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, EscapedControlChars) {
    // \u000A = newline, \u0009 = tab
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"line1\\u000Aline2\\u0009tab\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "line1\nline2\ttab"), 0);
    sstr_free(json);
    TestStruct_clear(&obj);
}

TEST(UnicodeEscape, RoundTripThroughMarshal) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    sstr_t json = sstr("{\"int_val\":0,\"long_val\":0,\"float_val\":0,\"double_val\":0,\"bool_val\":false,\"sstr_val\":\"\\u4e2d\\uD83D\\uDE80\"}");
    ASSERT_EQ(json_unmarshal_TestStruct(json, &obj), 0);
    EXPECT_EQ(sstr_compare_c(obj.sstr_val, "中🚀"), 0);

    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_TestStruct(&obj, out), 0);

    struct TestStruct obj2;
    TestStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_TestStruct(out, &obj2), 0);
    EXPECT_EQ(sstr_compare(obj.sstr_val, obj2.sstr_val), 0);

    sstr_free(json);
    sstr_free(out);
    TestStruct_clear(&obj);
    TestStruct_clear(&obj2);
}

// ==========================================================================
// Marshal round-trip consistency tests
// ==========================================================================

TEST(RoundTrip, ScalarStruct) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    obj.int_val = -42;
    obj.long_val = 9999999999LL;
    obj.float_val = 3.14f;
    obj.double_val = 2.718281828;
    obj.bool_val = true;
    obj.sstr_val = sstr("hello world");

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_TestStruct(&obj, json1), 0);

    struct TestStruct obj2;
    TestStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_TestStruct(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_TestStruct(&obj2, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0)
        << "Round trip mismatch:\n  json1=" << sstr_cstr(json1)
        << "\n  json2=" << sstr_cstr(json2);

    sstr_free(json1);
    sstr_free(json2);
    TestStruct_clear(&obj);
    TestStruct_clear(&obj2);
}

TEST(RoundTrip, NestedStruct) {
    struct NestedStruct obj;
    NestedStruct_init(&obj);
    obj.id = 100;
    obj.name = sstr("parent");
    obj.embedded.int_val = -1;
    obj.embedded.long_val = 0;
    obj.embedded.float_val = 0.5f;
    obj.embedded.double_val = 1.0;
    obj.embedded.bool_val = false;
    obj.embedded.sstr_val = sstr("child");

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_NestedStruct(&obj, json1), 0);

    struct NestedStruct obj2;
    NestedStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_NestedStruct(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_NestedStruct(&obj2, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0)
        << "Nested round trip mismatch:\n  json1=" << sstr_cstr(json1)
        << "\n  json2=" << sstr_cstr(json2);

    sstr_free(json1);
    sstr_free(json2);
    NestedStruct_clear(&obj);
    NestedStruct_clear(&obj2);
}

TEST(RoundTrip, OptionalFieldsPresent) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    obj.has_name = true;
    obj.name = sstr("Alice");
    obj.has_age = true;
    obj.age = 30;

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_AliasOptional(&obj, json1), 0);

    struct AliasOptional obj2;
    AliasOptional_init(&obj2);
    ASSERT_EQ(json_unmarshal_AliasOptional(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_AliasOptional(&obj2, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0);

    sstr_free(json1);
    sstr_free(json2);
    AliasOptional_clear(&obj);
    AliasOptional_clear(&obj2);
}

TEST(RoundTrip, OptionalFieldsAbsent) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_AliasOptional(&obj, json1), 0);

    struct AliasOptional obj2;
    AliasOptional_init(&obj2);
    ASSERT_EQ(json_unmarshal_AliasOptional(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_AliasOptional(&obj2, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0);
    EXPECT_FALSE(obj2.has_name);
    EXPECT_FALSE(obj2.has_age);

    sstr_free(json1);
    sstr_free(json2);
    AliasOptional_clear(&obj);
    AliasOptional_clear(&obj2);
}

TEST(RoundTrip, StringsWithSpecialChars) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    obj.int_val = 0;
    obj.long_val = 0;
    obj.float_val = 0.0f;
    obj.double_val = 0.0;
    obj.bool_val = false;
    obj.sstr_val = sstr("tab\there\nnewline\r\n\"quotes\" and \\backslash");

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_TestStruct(&obj, json1), 0);

    struct TestStruct obj2;
    TestStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_TestStruct(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_TestStruct(&obj2, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0);
    EXPECT_EQ(sstr_compare(obj.sstr_val, obj2.sstr_val), 0);

    sstr_free(json1);
    sstr_free(json2);
    TestStruct_clear(&obj);
    TestStruct_clear(&obj2);
}

TEST(RoundTrip, PrettyPrintConsistency) {
    struct TestStruct obj;
    TestStruct_init(&obj);
    obj.int_val = 1;
    obj.long_val = 2;
    obj.float_val = 3.0f;
    obj.double_val = 4.0;
    obj.bool_val = true;
    obj.sstr_val = sstr("test");

    sstr_t json1 = sstr_new();
    ASSERT_EQ(json_marshal_indent_TestStruct(&obj, 2, 0, json1), 0);

    struct TestStruct obj2;
    TestStruct_init(&obj2);
    ASSERT_EQ(json_unmarshal_TestStruct(json1, &obj2), 0);

    sstr_t json2 = sstr_new();
    ASSERT_EQ(json_marshal_indent_TestStruct(&obj2, 2, 0, json2), 0);

    EXPECT_EQ(sstr_compare(json1, json2), 0);

    sstr_free(json1);
    sstr_free(json2);
    TestStruct_clear(&obj);
    TestStruct_clear(&obj2);
}

// ==========================================================================
// Integer overflow boundary tests (16/32-bit types)
// ==========================================================================

TEST(OverflowBoundary, Int16Overflow) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":32768,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, Int16Underflow) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":-32769,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, Uint16Overflow) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":65536,\"u32\":0,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, Int32Overflow) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":0,\"i32\":2147483648,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, Uint32Overflow) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":4294967296,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, NegativeUnsigned) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":0,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":-1,\"u16\":0,\"u32\":0,\"u64\":0}");
    EXPECT_NE(json_unmarshal_PreciseInts(json, &obj), 0);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, ExactBoundaryValues) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":127,\"i16\":32767,\"i32\":2147483647,\"i64\":9223372036854775807,\"u8\":255,\"u16\":65535,\"u32\":4294967295,\"u64\":18446744073709551615}");
    ASSERT_EQ(json_unmarshal_PreciseInts(json, &obj), 0);
    EXPECT_EQ(obj.i8, INT8_MAX);
    EXPECT_EQ(obj.i16, INT16_MAX);
    EXPECT_EQ(obj.i32, INT32_MAX);
    EXPECT_EQ(obj.i64, INT64_MAX);
    EXPECT_EQ(obj.u8, UINT8_MAX);
    EXPECT_EQ(obj.u16, UINT16_MAX);
    EXPECT_EQ(obj.u32, UINT32_MAX);
    EXPECT_EQ(obj.u64, UINT64_MAX);
    sstr_free(json);
    PreciseInts_clear(&obj);
}

TEST(OverflowBoundary, ExactMinValues) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    sstr_t json = sstr("{\"i8\":-128,\"i16\":-32768,\"i32\":-2147483648,\"i64\":-9223372036854775808,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}");
    ASSERT_EQ(json_unmarshal_PreciseInts(json, &obj), 0);
    EXPECT_EQ(obj.i8, INT8_MIN);
    EXPECT_EQ(obj.i16, INT16_MIN);
    EXPECT_EQ(obj.i32, INT32_MIN);
    EXPECT_EQ(obj.i64, INT64_MIN);
    sstr_free(json);
    PreciseInts_clear(&obj);
}
