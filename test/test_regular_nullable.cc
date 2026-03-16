#include <gtest/gtest.h>
#include <stdio.h>
#include "json.gen.h"
#include "sstr.h"

TEST(RegularNullableTest, NullableHandling) {
    struct AliasOptional obj;
    AliasOptional_init(&obj);
    obj.has_age = true;
    obj.age = 99;

    sstr_t json = sstr("{\"age_years\":null}");
    int result = json_unmarshal_AliasOptional(json, &obj);
    
    printf("Result: %d\n", result);
    printf("has_age: %d, age: %d\n", obj.has_age, obj.age);
    
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(obj.has_age);  // Should be false after reading null
    
    sstr_free(json);
    AliasOptional_clear(&obj);
}
