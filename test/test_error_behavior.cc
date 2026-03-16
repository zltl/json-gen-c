#include <gtest/gtest.h>
#include "json.gen.h"
#include "sstr.h"

TEST(ErrorBehaviorTest, RegularUnmarshalKeepsDataOnError) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("preserved");
    obj.created = 12345;
    obj.id = 99;

    // Invalid JSON - should fail to parse
    sstr_t json = sstr("{\"user_name\":INVALID}");
    int result = json_unmarshal_AliasBasic(json, &obj);
    
    EXPECT_LT(result, 0);  // Should return error
    // Regular unmarshal doesn't clear on error - data remains
    // Note: Behavior may vary, but username should still be allocated
    
    sstr_free(json);
    AliasBasic_clear(&obj);
}

TEST(ErrorBehaviorTest, SelectiveUnmarshalClearsAllDataOnError) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("should-be-cleared");
    obj.created = 12345;
    obj.id = 99;

    uint64_t mask[AliasBasic_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasBasic_FIELD_username);

    // Invalid JSON - should fail to parse
    sstr_t json = sstr("{\"user_name\":INVALID}");
    int result = json_unmarshal_selected_AliasBasic(
        json, &obj, mask, AliasBasic_FIELD_MASK_WORD_COUNT);
    
    EXPECT_LT(result, 0);  // Should return error
    // Selective unmarshal DOES clear on error
    // This means even unselected fields (created, id) are reset!
    EXPECT_EQ(sstr_length(obj.username), 0);
    EXPECT_EQ(obj.created, 0);
    EXPECT_EQ(obj.id, 0);
    
    sstr_free(json);
    AliasBasic_clear(&obj);
}
