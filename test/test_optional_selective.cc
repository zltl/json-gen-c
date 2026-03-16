#include <gtest/gtest.h>
#include "json.gen.h"
#include "sstr.h"

TEST(SelectiveOptionalTest, OptionalFieldSelection) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    obj.has_name = true;
    obj.name = sstr("keep");
    obj.has_age = true;
    obj.age = 99;

    uint64_t mask[AliasOptional_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasOptional_FIELD_age);

    // Only update age, leave name alone
    sstr_t json = sstr("{\"display_name\":\"new-name\",\"age_years\":null}");
    ASSERT_EQ(json_unmarshal_selected_AliasOptional(
                  json, &obj, mask, AliasOptional_FIELD_MASK_WORD_COUNT), 0);

    // name should be unchanged
    EXPECT_TRUE(obj.has_name);
    EXPECT_EQ(sstr_compare_c(obj.name, "keep"), 0);
    // age should be updated to null
    EXPECT_FALSE(obj.has_age);

    sstr_free(json);
    AliasOptional_clear(&obj);
}
