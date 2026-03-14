extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>
#include <cstring>

class MapTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Basic map<sstr_t, int> =====

TEST_F(MapTest, InitClear) {
    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    EXPECT_EQ(obj.scores.entries, nullptr);
    EXPECT_EQ(obj.scores.len, 0);
    MapIntStruct_clear(&obj);
}

TEST_F(MapTest, MarshalEmpty) {
    struct MapIntStruct obj;
    MapIntStruct_init(&obj);

    sstr_t out = sstr_new();
    json_marshal_MapIntStruct(&obj, out);
    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "\"scores\":{}") != NULL) << "json: " << json;

    sstr_free(out);
    MapIntStruct_clear(&obj);
}

TEST_F(MapTest, MarshalIntMap) {
    struct MapIntStruct obj;
    MapIntStruct_init(&obj);

    struct json_map_entry_int entries[2];
    entries[0].key = sstr_of("alice", 5);
    entries[0].value = 95;
    entries[1].key = sstr_of("bob", 3);
    entries[1].value = 87;

    obj.scores.entries = entries;
    obj.scores.len = 2;

    sstr_t out = sstr_new();
    json_marshal_MapIntStruct(&obj, out);
    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "\"alice\":95") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"bob\":87") != NULL) << "json: " << json;

    sstr_free(out);
    // Don't clear entries since stack-allocated
    obj.scores.entries = NULL;
    obj.scores.len = 0;
    sstr_free(entries[0].key);
    sstr_free(entries[1].key);
    MapIntStruct_clear(&obj);
}

TEST_F(MapTest, UnmarshalIntMap) {
    const char* json_str = "{\"scores\":{\"alice\":95,\"bob\":87}}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    int r = json_unmarshal_MapIntStruct(input, &obj);
    EXPECT_EQ(r, 0) << "unmarshal failed";
    EXPECT_EQ(obj.scores.len, 2);
    ASSERT_NE(obj.scores.entries, nullptr);

    // Check entries (order should match JSON)
    EXPECT_STREQ(sstr_cstr(obj.scores.entries[0].key), "alice");
    EXPECT_EQ(obj.scores.entries[0].value, 95);
    EXPECT_STREQ(sstr_cstr(obj.scores.entries[1].key), "bob");
    EXPECT_EQ(obj.scores.entries[1].value, 87);

    sstr_free(input);
    MapIntStruct_clear(&obj);
}

TEST_F(MapTest, RoundtripIntMap) {
    const char* json_str = "{\"scores\":{\"x\":1,\"y\":2,\"z\":3}}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    int r = json_unmarshal_MapIntStruct(input, &obj);
    EXPECT_EQ(r, 0);

    sstr_t out = sstr_new();
    json_marshal_MapIntStruct(&obj, out);

    // Unmarshal again from marshaled output
    struct MapIntStruct obj2;
    MapIntStruct_init(&obj2);
    r = json_unmarshal_MapIntStruct(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.scores.len, 3);

    sstr_free(input);
    sstr_free(out);
    MapIntStruct_clear(&obj);
    MapIntStruct_clear(&obj2);
}

TEST_F(MapTest, UnmarshalEmptyMap) {
    const char* json_str = "{\"scores\":{}}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    int r = json_unmarshal_MapIntStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.scores.len, 0);

    sstr_free(input);
    MapIntStruct_clear(&obj);
}

// ===== All value types =====

TEST_F(MapTest, AllTypesRoundtrip) {
    const char* json_str =
        "{"
        "\"int_map\":{\"a\":42},"
        "\"long_map\":{\"b\":999999},"
        "\"float_map\":{\"c\":3.14},"
        "\"double_map\":{\"d\":2.718281828},"
        "\"bool_map\":{\"e\":true,\"f\":false},"
        "\"str_map\":{\"g\":\"hello\"},"
        "\"enum_map\":{\"h\":\"GREEN\"},"
        "\"struct_map\":{\"i\":{\"name\":\"John\",\"age\":\"30\"}}"
        "}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapAllTypesStruct obj;
    MapAllTypesStruct_init(&obj);
    int r = json_unmarshal_MapAllTypesStruct(input, &obj);
    EXPECT_EQ(r, 0) << "unmarshal failed";

    // Verify each map
    ASSERT_EQ(obj.int_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.int_map.entries[0].key), "a");
    EXPECT_EQ(obj.int_map.entries[0].value, 42);

    ASSERT_EQ(obj.long_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.long_map.entries[0].key), "b");
    EXPECT_EQ(obj.long_map.entries[0].value, 999999L);

    ASSERT_EQ(obj.float_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.float_map.entries[0].key), "c");
    EXPECT_NEAR(obj.float_map.entries[0].value, 3.14f, 0.01f);

    ASSERT_EQ(obj.double_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.double_map.entries[0].key), "d");
    EXPECT_NEAR(obj.double_map.entries[0].value, 2.718281828, 0.000001);

    ASSERT_EQ(obj.bool_map.len, 2);
    EXPECT_STREQ(sstr_cstr(obj.bool_map.entries[0].key), "e");
    EXPECT_EQ(obj.bool_map.entries[0].value, 1);
    EXPECT_STREQ(sstr_cstr(obj.bool_map.entries[1].key), "f");
    EXPECT_EQ(obj.bool_map.entries[1].value, 0);

    ASSERT_EQ(obj.str_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.str_map.entries[0].key), "g");
    EXPECT_STREQ(sstr_cstr(obj.str_map.entries[0].value), "hello");

    ASSERT_EQ(obj.enum_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.enum_map.entries[0].key), "h");
    EXPECT_EQ(obj.enum_map.entries[0].value, Color_GREEN);

    ASSERT_EQ(obj.struct_map.len, 1);
    EXPECT_STREQ(sstr_cstr(obj.struct_map.entries[0].key), "i");
    EXPECT_STREQ(sstr_cstr(obj.struct_map.entries[0].value.name), "John");
    EXPECT_STREQ(sstr_cstr(obj.struct_map.entries[0].value.age), "30");

    // Marshal and verify round-trip
    sstr_t out = sstr_new();
    json_marshal_MapAllTypesStruct(&obj, out);

    struct MapAllTypesStruct obj2;
    MapAllTypesStruct_init(&obj2);
    r = json_unmarshal_MapAllTypesStruct(out, &obj2);
    EXPECT_EQ(r, 0) << "re-unmarshal failed";
    EXPECT_EQ(obj2.int_map.len, 1);
    EXPECT_EQ(obj2.int_map.entries[0].value, 42);

    sstr_free(input);
    sstr_free(out);
    MapAllTypesStruct_clear(&obj);
    MapAllTypesStruct_clear(&obj2);
}

// ===== Array of maps =====

TEST_F(MapTest, ArrayOfMapsRoundtrip) {
    const char* json_str =
        "{\"tags\":[{\"x\":1,\"y\":2},{\"a\":10}]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapArrayStruct obj;
    MapArrayStruct_init(&obj);
    int r = json_unmarshal_MapArrayStruct(input, &obj);
    EXPECT_EQ(r, 0) << "unmarshal failed";
    EXPECT_EQ(obj.tags_len, 2);
    ASSERT_NE(obj.tags, nullptr);

    EXPECT_EQ(obj.tags[0].len, 2);
    EXPECT_STREQ(sstr_cstr(obj.tags[0].entries[0].key), "x");
    EXPECT_EQ(obj.tags[0].entries[0].value, 1);
    EXPECT_STREQ(sstr_cstr(obj.tags[0].entries[1].key), "y");
    EXPECT_EQ(obj.tags[0].entries[1].value, 2);

    EXPECT_EQ(obj.tags[1].len, 1);
    EXPECT_STREQ(sstr_cstr(obj.tags[1].entries[0].key), "a");
    EXPECT_EQ(obj.tags[1].entries[0].value, 10);

    // Marshal
    sstr_t out = sstr_new();
    json_marshal_MapArrayStruct(&obj, out);

    // Round-trip
    struct MapArrayStruct obj2;
    MapArrayStruct_init(&obj2);
    r = json_unmarshal_MapArrayStruct(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.tags_len, 2);
    EXPECT_EQ(obj2.tags[0].len, 2);
    EXPECT_EQ(obj2.tags[1].len, 1);

    sstr_free(input);
    sstr_free(out);
    MapArrayStruct_clear(&obj);
    MapArrayStruct_clear(&obj2);
}

TEST_F(MapTest, EmptyArrayOfMaps) {
    const char* json_str = "{\"tags\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapArrayStruct obj;
    MapArrayStruct_init(&obj);
    int r = json_unmarshal_MapArrayStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.tags_len, 0);

    sstr_free(input);
    MapArrayStruct_clear(&obj);
}

TEST_F(MapTest, NullMap) {
    const char* json_str = "{\"scores\":null}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    int r = json_unmarshal_MapIntStruct(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.scores.len, 0);
    EXPECT_EQ(obj.scores.entries, nullptr);

    sstr_free(input);
    MapIntStruct_clear(&obj);
}
