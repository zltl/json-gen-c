#include <gtest/gtest.h>
#include "json.gen.h"
#include "sstr.h"

// Test for potential memory leak when selectively parsing over existing strings
TEST(SelectiveMemoryTest, StringFieldReplacementFreesOld) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("original-long-string-that-should-be-freed");
    obj.created = 999;
    obj.id = 1;

    uint64_t mask[AliasBasic_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasBasic_FIELD_username);

    sstr_t json = sstr("{\"user_name\":\"new\",\"created_at\":42,\"id\":99}");
    ASSERT_EQ(json_unmarshal_selected_AliasBasic(
                  json, &obj, mask, AliasBasic_FIELD_MASK_WORD_COUNT), 0);

    // The old username should have been freed and replaced
    EXPECT_EQ(sstr_compare_c(obj.username, "new"), 0);
    // Unselected fields should remain unchanged
    EXPECT_EQ(obj.created, 999);
    EXPECT_EQ(obj.id, 1);

    sstr_free(json);
    AliasBasic_clear(&obj);
}

TEST(SelectiveMemoryTest, NestedStructReplacementFreesOld) {
    struct AliasNested obj;
    AliasNested_init(&obj);
    obj.info.name = sstr("old-name");
    obj.info.age = sstr("30");
    obj.addr.number = sstr("100");
    obj.addr.street = sstr("Old Street");

    uint64_t mask[AliasNested_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasNested_FIELD_addr);

    sstr_t json = sstr(
        "{\"user_info\":{\"name\":\"attacker\",\"age\":\"99\"},"
        "\"home_address\":{\"number\":\"42\",\"street\":\"New St\"}}");
    ASSERT_EQ(json_unmarshal_selected_AliasNested(
                  json, &obj, mask, AliasNested_FIELD_MASK_WORD_COUNT), 0);

    // info should remain unchanged
    EXPECT_EQ(sstr_compare_c(obj.info.name, "old-name"), 0);
    EXPECT_EQ(sstr_compare_c(obj.info.age, "30"), 0);
    // addr should be updated, with old strings freed
    EXPECT_EQ(sstr_compare_c(obj.addr.number, "42"), 0);
    EXPECT_EQ(sstr_compare_c(obj.addr.street, "New St"), 0);

    sstr_free(json);
    AliasNested_clear(&obj);
}
