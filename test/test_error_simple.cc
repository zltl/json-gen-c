#include <gtest/gtest.h>
#include <stdio.h>
#include "json.gen.h"
#include "sstr.h"

TEST(ErrorSimpleTest, SelectiveErrorClears) {
    struct AliasBasic obj;
    AliasBasic_init(&obj);
    obj.username = sstr("original");
    obj.created = 12345;
    obj.id = 99;

    printf("Before parse - username: %s, created: %ld, id: %d\n",
           sstr_cstr(obj.username), obj.created, obj.id);

    uint64_t mask[AliasBasic_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(mask, AliasBasic_FIELD_username);

    // Invalid JSON
    sstr_t json = sstr("{\"user_name\":INVALID}");
    int result = json_unmarshal_selected_AliasBasic(
        json, &obj, mask, AliasBasic_FIELD_MASK_WORD_COUNT);
    
    printf("Parse result: %d\n", result);
    printf("After parse - username ptr: %p, created: %ld, id: %d\n",
           (void*)obj.username, obj.created, obj.id);
    
    EXPECT_LT(result, 0);
    
    // After XXX_clear() is called on error, username becomes NULL/empty
    // Trying to access it may crash or show cleared data
    
    sstr_free(json);
    // Note: AliasBasic_clear already called by unmarshal on error
    // Calling it again might double-free!
}
