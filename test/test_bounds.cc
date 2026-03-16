#include <gtest/gtest.h>
#include "json.gen.h"
#include "sstr.h"

TEST(BoundsTest, FieldCountCorrect) {
    // AliasBasic has 3 fields: username, created, id
    EXPECT_EQ(AliasBasic_FIELD_COUNT, 3);
    EXPECT_EQ(AliasBasic_FIELD_MASK_WORD_COUNT, 1);
    
    // Check field index values
    EXPECT_EQ(AliasBasic_FIELD_username, 0);
    EXPECT_EQ(AliasBasic_FIELD_created, 1);
    EXPECT_EQ(AliasBasic_FIELD_id, 2);
}

TEST(BoundsTest, LargeFieldIndexesFitInMask) {
    // If we had 65 fields, we'd need 2 words
    int field_count = 65;
    int word_count = JSON_GEN_C_FIELD_MASK_WORD_COUNT(field_count);
    EXPECT_EQ(word_count, 2);
    
    field_count = 64;
    word_count = JSON_GEN_C_FIELD_MASK_WORD_COUNT(field_count);
    EXPECT_EQ(word_count, 1);
    
    field_count = 128;
    word_count = JSON_GEN_C_FIELD_MASK_WORD_COUNT(field_count);
    EXPECT_EQ(word_count, 2);
    
    field_count = 129;
    word_count = JSON_GEN_C_FIELD_MASK_WORD_COUNT(field_count);
    EXPECT_EQ(word_count, 3);
}
