extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

class DefaultValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---- DefaultBasic: int, long, float, double, bool, sstr_t defaults ----

TEST_F(DefaultValueTest, BasicIntDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_EQ(obj.count, 42);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicLongDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_EQ(obj.big, 1000000L);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicFloatDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_FLOAT_EQ(obj.ratio, 3.14f);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicDoubleDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_NEAR(obj.precise, 2.718281828, 1e-9);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicBoolDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_EQ(obj.active, true);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicStringDefault) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    ASSERT_NE(obj.label, nullptr);
    EXPECT_STREQ(sstr_cstr(obj.label), "hello");
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, BasicNoDefaultFieldIsZero) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);
    EXPECT_EQ(obj.no_default, 0);
    DefaultBasic_clear(&obj);
}

// ---- DefaultNegative: negative numbers ----

TEST_F(DefaultValueTest, NegativeInt) {
    struct DefaultNegative obj;
    DefaultNegative_init(&obj);
    EXPECT_EQ(obj.neg_int, -10);
    DefaultNegative_clear(&obj);
}

TEST_F(DefaultValueTest, NegativeLong) {
    struct DefaultNegative obj;
    DefaultNegative_init(&obj);
    EXPECT_EQ(obj.neg_long, -99999L);
    DefaultNegative_clear(&obj);
}

TEST_F(DefaultValueTest, NegativeFloat) {
    struct DefaultNegative obj;
    DefaultNegative_init(&obj);
    EXPECT_FLOAT_EQ(obj.neg_float, -1.5f);
    DefaultNegative_clear(&obj);
}

TEST_F(DefaultValueTest, NegativeDouble) {
    struct DefaultNegative obj;
    DefaultNegative_init(&obj);
    EXPECT_NEAR(obj.neg_double, -0.001, 1e-9);
    DefaultNegative_clear(&obj);
}

// ---- DefaultEnum: enum defaults ----

TEST_F(DefaultValueTest, EnumColorDefault) {
    struct DefaultEnum obj;
    DefaultEnum_init(&obj);
    EXPECT_EQ(obj.color, Color_GREEN);
    DefaultEnum_clear(&obj);
}

TEST_F(DefaultValueTest, EnumStatusDefault) {
    struct DefaultEnum obj;
    DefaultEnum_init(&obj);
    EXPECT_EQ(obj.status, Status_PENDING);
    DefaultEnum_clear(&obj);
}

// ---- DefaultOptional: optional fields with defaults ----

TEST_F(DefaultValueTest, OptionalIntDefault) {
    struct DefaultOptional obj;
    DefaultOptional_init(&obj);
    EXPECT_EQ(obj.score, 100);
    EXPECT_EQ(obj.has_score, false);
    DefaultOptional_clear(&obj);
}

TEST_F(DefaultValueTest, OptionalStringDefault) {
    struct DefaultOptional obj;
    DefaultOptional_init(&obj);
    ASSERT_NE(obj.name, nullptr);
    EXPECT_STREQ(sstr_cstr(obj.name), "anonymous");
    EXPECT_EQ(obj.has_name, false);
    DefaultOptional_clear(&obj);
}

// ---- DefaultPrecise: various int width types ----

TEST_F(DefaultValueTest, PreciseInt8) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.i8, -1);
    DefaultPrecise_clear(&obj);
}

TEST_F(DefaultValueTest, PreciseInt16) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.i16, 256);
    DefaultPrecise_clear(&obj);
}

TEST_F(DefaultValueTest, PreciseInt32) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.i32, -100000);
    DefaultPrecise_clear(&obj);
}

TEST_F(DefaultValueTest, PreciseUint8) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.u8, 255);
    DefaultPrecise_clear(&obj);
}

TEST_F(DefaultValueTest, PreciseUint16) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.u16, 65535);
    DefaultPrecise_clear(&obj);
}

TEST_F(DefaultValueTest, PreciseUint32) {
    struct DefaultPrecise obj;
    DefaultPrecise_init(&obj);
    EXPECT_EQ(obj.u32, 42U);
    DefaultPrecise_clear(&obj);
}

// ---- DefaultMixed: alias + default + no-default ----

TEST_F(DefaultValueTest, MixedCountDefault) {
    struct DefaultMixed obj;
    DefaultMixed_init(&obj);
    EXPECT_EQ(obj.count, 10);
    DefaultMixed_clear(&obj);
}

TEST_F(DefaultValueTest, MixedNameNoDefault) {
    struct DefaultMixed obj;
    DefaultMixed_init(&obj);
    EXPECT_EQ(obj.name, nullptr);
    DefaultMixed_clear(&obj);
}

TEST_F(DefaultValueTest, MixedBoolDefault) {
    struct DefaultMixed obj;
    DefaultMixed_init(&obj);
    EXPECT_EQ(obj.enabled, false);
    DefaultMixed_clear(&obj);
}

// ---- Marshal/Unmarshal with defaults ----

TEST_F(DefaultValueTest, MarshalDefaultValues) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);

    sstr_t out = sstr_new();
    json_marshal_DefaultBasic(&obj, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"count\":42") != NULL) << json;
    EXPECT_TRUE(strstr(json, "\"big\":1000000") != NULL) << json;
    EXPECT_TRUE(strstr(json, "\"active\":true") != NULL) << json;
    EXPECT_TRUE(strstr(json, "\"label\":\"hello\"") != NULL) << json;
    EXPECT_TRUE(strstr(json, "\"no_default\":0") != NULL) << json;

    sstr_free(out);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, UnmarshalOverridesDefaults) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);

    const char* input = "{\"count\":99,\"big\":0,\"ratio\":0.0,\"precise\":0.0,"
                         "\"active\":false,\"label\":\"world\",\"no_default\":7}";
    sstr_t json = sstr_of(input, strlen(input));
    int r = json_unmarshal_DefaultBasic(json, &obj);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj.count, 99);
    EXPECT_EQ(obj.big, 0L);
    EXPECT_FLOAT_EQ(obj.ratio, 0.0f);
    EXPECT_EQ(obj.active, false);
    EXPECT_STREQ(sstr_cstr(obj.label), "world");
    EXPECT_EQ(obj.no_default, 7);

    sstr_free(json);
    DefaultBasic_clear(&obj);
}

TEST_F(DefaultValueTest, MarshalMixedWithAlias) {
    struct DefaultMixed obj;
    DefaultMixed_init(&obj);

    sstr_t out = sstr_new();
    json_marshal_DefaultMixed(&obj, out);
    const char* json = sstr_cstr(out);

    // The JSON key should be the alias "user_count", not "count"
    EXPECT_TRUE(strstr(json, "\"user_count\":10") != NULL) << json;
    EXPECT_TRUE(strstr(json, "\"enabled\":false") != NULL) << json;

    sstr_free(out);
    DefaultMixed_clear(&obj);
}

TEST_F(DefaultValueTest, UnmarshalMixedWithAlias) {
    struct DefaultMixed obj;
    DefaultMixed_init(&obj);

    const char* input = "{\"user_count\":77,\"name\":\"test\",\"enabled\":true}";
    sstr_t json = sstr_of(input, strlen(input));
    int r = json_unmarshal_DefaultMixed(json, &obj);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj.count, 77);
    EXPECT_STREQ(sstr_cstr(obj.name), "test");
    EXPECT_EQ(obj.enabled, true);

    sstr_free(json);
    DefaultMixed_clear(&obj);
}

TEST_F(DefaultValueTest, RoundtripWithDefaults) {
    struct DefaultBasic obj;
    DefaultBasic_init(&obj);

    // Marshal defaults
    sstr_t out = sstr_new();
    json_marshal_DefaultBasic(&obj, out);

    // Unmarshal back into a fresh struct
    struct DefaultBasic obj2;
    DefaultBasic_init(&obj2);
    int r = json_unmarshal_DefaultBasic(out, &obj2);
    EXPECT_EQ(r, 0);

    EXPECT_EQ(obj2.count, 42);
    EXPECT_EQ(obj2.big, 1000000L);
    EXPECT_FLOAT_EQ(obj2.ratio, 3.14f);
    EXPECT_NEAR(obj2.precise, 2.718281828, 1e-9);
    EXPECT_EQ(obj2.active, true);
    EXPECT_STREQ(sstr_cstr(obj2.label), "hello");
    EXPECT_EQ(obj2.no_default, 0);

    sstr_free(out);
    DefaultBasic_clear(&obj);
    DefaultBasic_clear(&obj2);
}
