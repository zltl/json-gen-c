#include <gtest/gtest.h>

#include <stdlib.h>
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

TEST(SelectiveParseTest, NullableNullClearsSelectedField) {
    struct NullableOnlyStruct obj;
    NullableOnlyStruct_init(&obj);
    obj.id = 1;
    obj.has_name = true;
    obj.name = sstr("old-name");

    uint64_t mask[NullableOnlyStruct_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, NullableOnlyStruct_FIELD_name);

    sstr_t json = sstr("{\"name\":null}");
    ASSERT_EQ(json_unmarshal_selected_NullableOnlyStruct(
                  json, &obj, mask, NullableOnlyStruct_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_FALSE(obj.has_name);
    EXPECT_EQ(obj.name, nullptr);

    sstr_free(json);
    NullableOnlyStruct_clear(&obj);
}

TEST(SelectiveParseTest, ReplacesDynamicArrayField) {
    struct AliasArray obj;
    AliasArray_init(&obj);
    obj.tags_len = 3;
    obj.tags = (sstr_t*)malloc(sizeof(sstr_t) * 3);
    obj.tags[0] = sstr("old-a");
    obj.tags[1] = sstr("old-b");
    obj.tags[2] = sstr("old-c");

    uint64_t mask[AliasArray_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasArray_FIELD_tags);

    sstr_t json = sstr("{\"tag_list\":[\"new-tag\"]}");
    ASSERT_EQ(json_unmarshal_selected_AliasArray(
                  json, &obj, mask, AliasArray_FIELD_MASK_WORD_COUNT),
              0);

    ASSERT_EQ(obj.tags_len, 1);
    EXPECT_EQ(sstr_compare_c(obj.tags[0], "new-tag"), 0);

    sstr_free(json);
    AliasArray_clear(&obj);
}

TEST(SelectiveParseTest, ReplacesFixedArrayFieldAndClearsRemainder) {
    struct AliasArray obj;
    AliasArray_init(&obj);
    obj.scores[0] = 10;
    obj.scores[1] = 20;
    obj.scores[2] = 30;

    uint64_t mask[AliasArray_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasArray_FIELD_scores);

    sstr_t json = sstr("{\"score_list\":[7]}");
    ASSERT_EQ(json_unmarshal_selected_AliasArray(
                  json, &obj, mask, AliasArray_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_EQ(obj.scores[0], 7);
    EXPECT_EQ(obj.scores[1], 0);
    EXPECT_EQ(obj.scores[2], 0);

    sstr_free(json);
    AliasArray_clear(&obj);
}

TEST(SelectiveParseTest, SelectedNestedStructReplacesWholeField) {
    struct AliasNested obj;
    AliasNested_init(&obj);
    obj.info.name = sstr("keep-user");
    obj.info.age = sstr("21");
    obj.addr.number = sstr("10");
    obj.addr.street = sstr("Old Street");

    uint64_t mask[AliasNested_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasNested_FIELD_addr);

    sstr_t json = sstr("{\"home_address\":{\"number\":\"42\"}}");
    ASSERT_EQ(json_unmarshal_selected_AliasNested(
                  json, &obj, mask, AliasNested_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_EQ(sstr_compare_c(obj.info.name, "keep-user"), 0);
    EXPECT_EQ(sstr_compare_c(obj.info.age, "21"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.number, "42"), 0);
    EXPECT_EQ(obj.addr.street, nullptr);

    sstr_free(json);
    AliasNested_clear(&obj);
}

TEST(SelectiveParseTest, ReplacesMapField) {
    struct MapIntStruct obj;
    MapIntStruct_init(&obj);
    obj.scores.len = 2;
    obj.scores.entries = (struct json_map_entry_int*)malloc(
        sizeof(struct json_map_entry_int) * 2);
    obj.scores.entries[0].key = sstr("old-a");
    obj.scores.entries[0].value = 1;
    obj.scores.entries[1].key = sstr("old-b");
    obj.scores.entries[1].value = 2;

    uint64_t mask[MapIntStruct_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, MapIntStruct_FIELD_scores);

    sstr_t json = sstr("{\"scores\":{\"new-key\":9}}");
    ASSERT_EQ(json_unmarshal_selected_MapIntStruct(
                  json, &obj, mask, MapIntStruct_FIELD_MASK_WORD_COUNT),
              0);

    ASSERT_EQ(obj.scores.len, 1);
    EXPECT_EQ(sstr_compare_c(obj.scores.entries[0].key, "new-key"), 0);
    EXPECT_EQ(obj.scores.entries[0].value, 9);

    sstr_free(json);
    MapIntStruct_clear(&obj);
}

TEST(SelectiveParseTest, ReplacesOneofField) {
    struct Drawing obj;
    Drawing_init(&obj);
    obj.name = sstr("keep-name");
    obj.shape.tag = Shape_triangle;
    obj.shape.value.triangle.base = 1.0f;
    obj.shape.value.triangle.height = 2.0f;
    obj.shape.value.triangle.label = sstr("old-triangle");

    uint64_t mask[Drawing_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, Drawing_FIELD_shape);

    sstr_t json = sstr(
        "{\"shape\":{\"type\":\"rectangle\",\"width\":8.5,\"height\":3.5}}");
    ASSERT_EQ(json_unmarshal_selected_Drawing(
                  json, &obj, mask, Drawing_FIELD_MASK_WORD_COUNT),
              0);

    EXPECT_EQ(sstr_compare_c(obj.name, "keep-name"), 0);
    EXPECT_EQ(obj.shape.tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(obj.shape.value.rectangle.width, 8.5f);
    EXPECT_FLOAT_EQ(obj.shape.value.rectangle.height, 3.5f);

    sstr_free(json);
    Drawing_clear(&obj);
}
