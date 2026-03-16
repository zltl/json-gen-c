#include <gtest/gtest.h>

#include <stdint.h>

#include "json.gen.h"
#include "sstr.h"

TEST(SelectiveParseTest, UpdatesOnlySelectedAliasedField) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("alice");
    obj.created = 1234567890L;
    obj.id = 7;

    uint64_t mask[AliasBasic_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasBasic_FIELD_username);

    sstr_t json = sstr("{\"user_name\":\"bob\",\"created_at\":42,\"id\":99}");
    ASSERT_EQ(json_unmarshal_selected_AliasBasic(
                  json, &obj, mask, AliasBasic_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_EQ(sstr_compare_c(obj.username, "bob"), 0);
    EXPECT_EQ(obj.created, 1234567890L);
    EXPECT_EQ(obj.id, 7);

    sstr_free(json);
    AliasBasic_clear(&obj);
}

TEST(SelectiveParseTest, LeavesAbsentSelectedFieldsUnchanged) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    obj.has_name = true;
    obj.name = sstr("existing-name");
    obj.has_age = true;
    obj.age = 55;

    uint64_t mask[AliasOptional_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasOptional_FIELD_name);
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasOptional_FIELD_age);

    sstr_t json = sstr("{\"display_name\":\"updated-name\"}");
    ASSERT_EQ(json_unmarshal_selected_AliasOptional(
                  json, &obj, mask, AliasOptional_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_TRUE(obj.has_name);
    EXPECT_EQ(sstr_compare_c(obj.name, "updated-name"), 0);
    EXPECT_TRUE(obj.has_age);
    EXPECT_EQ(obj.age, 55);

    sstr_free(json);
    AliasOptional_clear(&obj);
}

TEST(SelectiveParseTest, SkipsUnselectedNestedAliasFields) {
    struct AliasNested obj;
    AliasNested_init(&obj);
    obj.info.name = sstr("keep-user");
    obj.info.age = sstr("21");
    obj.addr.number = sstr("10");
    obj.addr.street = sstr("Old Street");

    uint64_t mask[AliasNested_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasNested_FIELD_addr);

    sstr_t json = sstr(
        "{\"user_info\":{\"name\":\"new-user\",\"age\":\"44\"},"
        "\"home_address\":{\"number\":\"42\",\"street\":\"Main St\"}}");
    ASSERT_EQ(json_unmarshal_selected_AliasNested(
                  json, &obj, mask, AliasNested_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_EQ(sstr_compare_c(obj.info.name, "keep-user"), 0);
    EXPECT_EQ(sstr_compare_c(obj.info.age, "21"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.number, "42"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.street, "Main St"), 0);

    sstr_free(json);
    AliasNested_clear(&obj);
}

TEST(SelectiveParseTest, RejectsNullOrShortMasks) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);

    uint64_t mask[AliasBasic_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasBasic_FIELD_username);

    sstr_t json = sstr("{\"user_name\":\"bob\"}");
    EXPECT_EQ(json_unmarshal_selected_AliasBasic(json, &obj, NULL, 1), -1);
    EXPECT_EQ(json_unmarshal_selected_AliasBasic(json, &obj, mask, 0), -1);

    sstr_free(json);
    AliasBasic_clear(&obj);
}
