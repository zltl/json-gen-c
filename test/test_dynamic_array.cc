#include <gtest/gtest.h>
#include "json.gen.h"
#include "sstr.h"

TEST(SelectiveDynamicArrayTest, UpdatesDynamicArrayField) {
    struct AliasArray obj;
    AliasArray_init(&obj);
    obj.tags = (sstr_t*)malloc(2 * sizeof(sstr_t));
    obj.tags[0] = sstr("tag1");
    obj.tags[1] = sstr("tag2");
    obj.tags_len = 2;
    obj.scores[0] = 100;
    obj.scores[1] = 200;
    obj.scores[2] = 300;

    uint64_t mask[AliasArray_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasArray_FIELD_tags);

    sstr_t json = sstr("{\"tag_list\":[\"a\",\"b\",\"c\"],\"score_list\":[1,2,3]}");
    ASSERT_EQ(json_unmarshal_selected_AliasArray(
                  json, &obj, mask, AliasArray_FIELD_MASK_WORD_COUNT), 0);

    EXPECT_EQ(obj.tags_len, 3);
    EXPECT_EQ(sstr_compare_c(obj.tags[0], "a"), 0);
    // scores should remain unchanged
    EXPECT_EQ(obj.scores[0], 100);
    EXPECT_EQ(obj.scores[1], 200);

    sstr_free(json);
    AliasArray_clear(&obj);
}
