/**
 * @file alias_test.cc
 * @brief Tests for @json field alias annotation support
 */

#include <gtest/gtest.h>
#include <string.h>

#include "json.gen.h"
#include "sstr.h"

class AliasTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Basic alias: marshal uses alias names
TEST_F(AliasTest, MarshalUsesAliasNames) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("alice");
    obj.created = 1700000000;
    obj.id = 42;

    sstr_t json = sstr_new();
    ASSERT_EQ(json_marshal_AliasBasic(&obj, json), 0);

    const char* s = sstr_cstr(json);
    EXPECT_TRUE(strstr(s, "\"user_name\"") != nullptr)
        << "Expected alias 'user_name' in JSON: " << s;
    EXPECT_TRUE(strstr(s, "\"created_at\"") != nullptr)
        << "Expected alias 'created_at' in JSON: " << s;
    EXPECT_TRUE(strstr(s, "\"id\"") != nullptr)
        << "Expected field 'id' (no alias) in JSON: " << s;
    // C field name should NOT appear as JSON key
    EXPECT_TRUE(strstr(s, "\"username\"") == nullptr)
        << "C field name 'username' should not appear in JSON: " << s;
    EXPECT_TRUE(strstr(s, "\"created\"") == nullptr)
        << "C field name 'created' should not appear in JSON: " << s;

    AliasBasic_clear(&obj);
    sstr_free(json);
}

// Basic alias: unmarshal with alias names
TEST_F(AliasTest, UnmarshalWithAliasNames) {
    const char* json_str =
        "{\"user_name\":\"bob\",\"created_at\":1234567890,\"id\":7}";
    sstr_t json = sstr(json_str);

    struct AliasBasic obj;
    AliasBasic_init(&obj);
    ASSERT_EQ(json_unmarshal_AliasBasic(json, &obj), 0);

    EXPECT_EQ(sstr_compare_c(obj.username, "bob"), 0);
    EXPECT_EQ(obj.created, 1234567890);
    EXPECT_EQ(obj.id, 7);

    AliasBasic_clear(&obj);
    sstr_free(json);
}

// Round-trip: marshal then unmarshal preserves data
TEST_F(AliasTest, RoundTrip) {
    struct AliasBasic orig;
    AliasBasic_init(&orig);
    orig.username = sstr("charlie");
    orig.created = 9999999999L;
    orig.id = 100;

    sstr_t json = sstr_new();
    ASSERT_EQ(json_marshal_AliasBasic(&orig, json), 0);

    struct AliasBasic copy;
    AliasBasic_init(&copy);
    ASSERT_EQ(json_unmarshal_AliasBasic(json, &copy), 0);

    EXPECT_EQ(sstr_compare(orig.username, copy.username), 0);
    EXPECT_EQ(orig.created, copy.created);
    EXPECT_EQ(orig.id, copy.id);

    AliasBasic_clear(&orig);
    AliasBasic_clear(&copy);
    sstr_free(json);
}

// Mixed: some fields with alias, some without
TEST_F(AliasTest, MixedAliasAndNonAlias) {
    struct AliasMixed obj;
    AliasMixed_init(&obj);
    obj.first = sstr("Jane");
    obj.last = sstr("Doe");
    obj.active = true;

    sstr_t json = sstr_new();
    ASSERT_EQ(json_marshal_AliasMixed(&obj, json), 0);

    const char* s = sstr_cstr(json);
    EXPECT_TRUE(strstr(s, "\"first_name\"") != nullptr);
    EXPECT_TRUE(strstr(s, "\"last\"") != nullptr);
    EXPECT_TRUE(strstr(s, "\"is_active\"") != nullptr);

    // Unmarshal round-trip
    struct AliasMixed copy;
    AliasMixed_init(&copy);
    ASSERT_EQ(json_unmarshal_AliasMixed(json, &copy), 0);

    EXPECT_EQ(sstr_compare(obj.first, copy.first), 0);
    EXPECT_EQ(sstr_compare(obj.last, copy.last), 0);
    EXPECT_EQ(obj.active, copy.active);

    AliasMixed_clear(&obj);
    AliasMixed_clear(&copy);
    sstr_free(json);
}

// Alias with optional/nullable modifiers
TEST_F(AliasTest, AliasWithOptional) {
    // Test with values present
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    obj.has_name = true;
    obj.name = sstr("display");
    obj.has_age = true;
    obj.age = 25;

    sstr_t json = sstr_new();
    ASSERT_EQ(json_marshal_AliasOptional(&obj, json), 0);

    const char* s = sstr_cstr(json);
    EXPECT_TRUE(strstr(s, "\"display_name\"") != nullptr);
    EXPECT_TRUE(strstr(s, "\"age_years\"") != nullptr);

    struct AliasOptional copy;
    AliasOptional_init(&copy);
    ASSERT_EQ(json_unmarshal_AliasOptional(json, &copy), 0);
    EXPECT_TRUE(copy.has_name);
    EXPECT_EQ(sstr_compare(obj.name, copy.name), 0);
    EXPECT_TRUE(copy.has_age);
    EXPECT_EQ(copy.age, 25);

    AliasOptional_clear(&obj);
    AliasOptional_clear(&copy);
    sstr_free(json);
}

// Alias with optional fields omitted
TEST_F(AliasTest, AliasOptionalOmitted) {
    const char* json_str = "{}";
    sstr_t json = sstr(json_str);

    struct AliasOptional obj;
    AliasOptional_init(&obj);
    ASSERT_EQ(json_unmarshal_AliasOptional(json, &obj), 0);
    EXPECT_FALSE(obj.has_name);
    EXPECT_FALSE(obj.has_age);

    AliasOptional_clear(&obj);
    sstr_free(json);
}

// Alias with arrays
TEST_F(AliasTest, AliasWithDynamicArray) {
    const char* json_str = "{\"tag_list\":[\"a\",\"b\",\"c\"],\"score_list\":[10,20,30]}";
    sstr_t json = sstr(json_str);

    struct AliasArray obj;
    AliasArray_init(&obj);
    ASSERT_EQ(json_unmarshal_AliasArray(json, &obj), 0);

    EXPECT_EQ(obj.tags_len, 3);
    EXPECT_EQ(sstr_compare_c(obj.tags[0], "a"), 0);
    EXPECT_EQ(sstr_compare_c(obj.tags[1], "b"), 0);
    EXPECT_EQ(sstr_compare_c(obj.tags[2], "c"), 0);
    EXPECT_EQ(obj.scores[0], 10);
    EXPECT_EQ(obj.scores[1], 20);
    EXPECT_EQ(obj.scores[2], 30);

    // Marshal and check alias names in output
    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_AliasArray(&obj, out), 0);
    const char* s = sstr_cstr(out);
    EXPECT_TRUE(strstr(s, "\"tag_list\"") != nullptr);
    EXPECT_TRUE(strstr(s, "\"score_list\"") != nullptr);

    AliasArray_clear(&obj);
    sstr_free(json);
    sstr_free(out);
}

// Alias with nested structs
TEST_F(AliasTest, AliasWithNestedStruct) {
    const char* json_str =
        "{\"user_info\":{\"name\":\"Eve\",\"age\":\"30\"},"
        "\"home_address\":{\"number\":\"42\",\"street\":\"Main St\"}}";
    sstr_t json = sstr(json_str);

    struct AliasNested obj;
    AliasNested_init(&obj);
    ASSERT_EQ(json_unmarshal_AliasNested(json, &obj), 0);

    EXPECT_EQ(sstr_compare_c(obj.info.name, "Eve"), 0);
    EXPECT_EQ(sstr_compare_c(obj.info.age, "30"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.number, "42"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.street, "Main St"), 0);

    // Marshal round-trip
    sstr_t out = sstr_new();
    ASSERT_EQ(json_marshal_AliasNested(&obj, out), 0);
    const char* s = sstr_cstr(out);
    EXPECT_TRUE(strstr(s, "\"user_info\"") != nullptr);
    EXPECT_TRUE(strstr(s, "\"home_address\"") != nullptr);

    AliasNested_clear(&obj);
    sstr_free(json);
    sstr_free(out);
}

// Unmarshal with C field name should fail when alias is defined
TEST_F(AliasTest, UnmarshalWithCFieldNameFails) {
    // Use C field names instead of aliases — parser rejects unknown keys
    const char* json_str = "{\"username\":\"test\",\"created\":999,\"id\":1}";
    sstr_t json = sstr(json_str);

    struct AliasBasic obj;
    AliasBasic_init(&obj);
    // "username" and "created" are not recognized (alias overrides the key)
    EXPECT_NE(json_unmarshal_AliasBasic(json, &obj), 0);

    AliasBasic_clear(&obj);
    sstr_free(json);
}
