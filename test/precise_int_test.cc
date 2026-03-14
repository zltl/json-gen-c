extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>
#include <cstring>
#include <climits>
#include <cstdint>

class PreciseIntTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Scalar marshal/unmarshal round-trip =====

TEST_F(PreciseIntTest, MarshalAllTypes) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    obj.i8 = -100;
    obj.i16 = -30000;
    obj.i32 = -2000000000;
    obj.i64 = -9000000000000000000LL;
    obj.u8 = 200;
    obj.u16 = 60000;
    obj.u32 = 4000000000U;
    obj.u64 = 18000000000000000000ULL;

    sstr_t out = sstr_new();
    json_marshal_PreciseInts(&obj, out);

    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "-100") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "-30000") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "-2000000000") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "-9000000000000000000") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "200") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "60000") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "4000000000") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "18000000000000000000") != NULL) << "json: " << json;

    sstr_free(out);
    PreciseInts_clear(&obj);
}

TEST_F(PreciseIntTest, UnmarshalAllTypes) {
    const char* json_str =
        "{\"i8\":-100,\"i16\":-30000,\"i32\":-2000000000,"
        "\"i64\":-9000000000000000000,"
        "\"u8\":200,\"u16\":60000,\"u32\":4000000000,"
        "\"u64\":18000000000000000000}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct PreciseInts obj;
    PreciseInts_init(&obj);
    int r = json_unmarshal_PreciseInts(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i8, -100);
    EXPECT_EQ(obj.i16, -30000);
    EXPECT_EQ(obj.i32, -2000000000);
    EXPECT_EQ(obj.i64, -9000000000000000000LL);
    EXPECT_EQ(obj.u8, 200);
    EXPECT_EQ(obj.u16, 60000);
    EXPECT_EQ(obj.u32, 4000000000U);
    EXPECT_EQ(obj.u64, 18000000000000000000ULL);

    sstr_free(input);
    PreciseInts_clear(&obj);
}

TEST_F(PreciseIntTest, RoundTrip) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    obj.i8 = INT8_MIN;
    obj.i16 = INT16_MAX;
    obj.i32 = INT32_MIN;
    obj.i64 = INT64_MAX;
    obj.u8 = UINT8_MAX;
    obj.u16 = 0;
    obj.u32 = UINT32_MAX;
    obj.u64 = UINT64_MAX;

    sstr_t out = sstr_new();
    json_marshal_PreciseInts(&obj, out);

    struct PreciseInts obj2;
    PreciseInts_init(&obj2);
    int r = json_unmarshal_PreciseInts(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.i8, INT8_MIN);
    EXPECT_EQ(obj2.i16, INT16_MAX);
    EXPECT_EQ(obj2.i32, INT32_MIN);
    EXPECT_EQ(obj2.i64, INT64_MAX);
    EXPECT_EQ(obj2.u8, UINT8_MAX);
    EXPECT_EQ(obj2.u16, (uint16_t)0);
    EXPECT_EQ(obj2.u32, UINT32_MAX);
    EXPECT_EQ(obj2.u64, UINT64_MAX);

    sstr_free(out);
    PreciseInts_clear(&obj);
    PreciseInts_clear(&obj2);
}

// ===== Boundary and error cases =====

TEST_F(PreciseIntTest, Int8Boundaries) {
    // INT8_MIN = -128, INT8_MAX = 127
    const char* ok_min = "{\"i8\":-128,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}";
    sstr_t input = sstr_of(ok_min, strlen(ok_min));
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    EXPECT_EQ(json_unmarshal_PreciseInts(input, &obj), 0);
    EXPECT_EQ(obj.i8, INT8_MIN);
    sstr_free(input);
    PreciseInts_clear(&obj);

    const char* ok_max = "{\"i8\":127,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}";
    input = sstr_of(ok_max, strlen(ok_max));
    PreciseInts_init(&obj);
    EXPECT_EQ(json_unmarshal_PreciseInts(input, &obj), 0);
    EXPECT_EQ(obj.i8, INT8_MAX);
    sstr_free(input);
    PreciseInts_clear(&obj);

    // Out of range: 128
    const char* bad = "{\"i8\":128,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}";
    input = sstr_of(bad, strlen(bad));
    PreciseInts_init(&obj);
    EXPECT_NE(json_unmarshal_PreciseInts(input, &obj), 0);
    sstr_free(input);
    PreciseInts_clear(&obj);

    // Out of range: -129
    const char* bad2 = "{\"i8\":-129,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":0,\"u16\":0,\"u32\":0,\"u64\":0}";
    input = sstr_of(bad2, strlen(bad2));
    PreciseInts_init(&obj);
    EXPECT_NE(json_unmarshal_PreciseInts(input, &obj), 0);
    sstr_free(input);
    PreciseInts_clear(&obj);
}

TEST_F(PreciseIntTest, Uint8Boundaries) {
    const char* ok_max = "{\"i8\":0,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":255,\"u16\":0,\"u32\":0,\"u64\":0}";
    sstr_t input = sstr_of(ok_max, strlen(ok_max));
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    EXPECT_EQ(json_unmarshal_PreciseInts(input, &obj), 0);
    EXPECT_EQ(obj.u8, UINT8_MAX);
    sstr_free(input);
    PreciseInts_clear(&obj);

    // Out of range: 256
    const char* bad = "{\"i8\":0,\"i16\":0,\"i32\":0,\"i64\":0,\"u8\":256,\"u16\":0,\"u32\":0,\"u64\":0}";
    input = sstr_of(bad, strlen(bad));
    PreciseInts_init(&obj);
    EXPECT_NE(json_unmarshal_PreciseInts(input, &obj), 0);
    sstr_free(input);
    PreciseInts_clear(&obj);
}

TEST_F(PreciseIntTest, InitClear) {
    struct PreciseInts obj;
    PreciseInts_init(&obj);
    EXPECT_EQ(obj.i8, 0);
    EXPECT_EQ(obj.i16, 0);
    EXPECT_EQ(obj.i32, 0);
    EXPECT_EQ(obj.i64, 0);
    EXPECT_EQ(obj.u8, 0);
    EXPECT_EQ(obj.u16, 0);
    EXPECT_EQ(obj.u32, 0U);
    EXPECT_EQ(obj.u64, 0ULL);
    PreciseInts_clear(&obj);

    // After clear all should be 0
    EXPECT_EQ(obj.i8, 0);
    EXPECT_EQ(obj.u64, 0ULL);
}

// ===== Fixed array tests =====

TEST_F(PreciseIntTest, FixedArrayMarshalUnmarshal) {
    struct PreciseIntArrays obj;
    PreciseIntArrays_init(&obj);
    obj.i8_arr[0] = -10;
    obj.i8_arr[1] = 0;
    obj.i8_arr[2] = 50;
    obj.i8_arr[3] = 127;
    obj.u64_fixed[0] = 1;
    obj.u64_fixed[1] = UINT64_MAX;

    sstr_t out = sstr_new();
    json_marshal_PreciseIntArrays(&obj, out);

    const char* json = sstr_cstr(out);
    EXPECT_TRUE(strstr(json, "\"i8_arr\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "127") != NULL) << "json: " << json;

    // Round trip
    struct PreciseIntArrays obj2;
    PreciseIntArrays_init(&obj2);
    int r = json_unmarshal_PreciseIntArrays(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.i8_arr[0], -10);
    EXPECT_EQ(obj2.i8_arr[1], 0);
    EXPECT_EQ(obj2.i8_arr[2], 50);
    EXPECT_EQ(obj2.i8_arr[3], 127);
    EXPECT_EQ(obj2.u64_fixed[0], 1ULL);
    EXPECT_EQ(obj2.u64_fixed[1], UINT64_MAX);

    sstr_free(out);
    PreciseIntArrays_clear(&obj);
    PreciseIntArrays_clear(&obj2);
}

// ===== Dynamic array tests =====

TEST_F(PreciseIntTest, DynArrayMarshalUnmarshal) {
    struct PreciseIntArrays obj;
    PreciseIntArrays_init(&obj);

    uint32_t u32_vals[] = {0, 100, UINT32_MAX};
    obj.u32_dyn = u32_vals;
    obj.u32_dyn_len = 3;

    int64_t i64_vals[] = {INT64_MIN, 0, INT64_MAX};
    obj.i64_dyn = i64_vals;
    obj.i64_dyn_len = 3;

    sstr_t out = sstr_new();
    json_marshal_PreciseIntArrays(&obj, out);

    // unmarshal
    struct PreciseIntArrays obj2;
    PreciseIntArrays_init(&obj2);
    int r = json_unmarshal_PreciseIntArrays(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.u32_dyn_len, 3);
    EXPECT_EQ(obj2.u32_dyn[0], 0U);
    EXPECT_EQ(obj2.u32_dyn[1], 100U);
    EXPECT_EQ(obj2.u32_dyn[2], UINT32_MAX);
    EXPECT_EQ(obj2.i64_dyn_len, 3);
    EXPECT_EQ(obj2.i64_dyn[0], INT64_MIN);
    EXPECT_EQ(obj2.i64_dyn[1], 0LL);
    EXPECT_EQ(obj2.i64_dyn[2], INT64_MAX);

    sstr_free(out);
    // Don't clear obj since arrays are stack-allocated
    obj.u32_dyn = NULL;
    obj.u32_dyn_len = 0;
    obj.i64_dyn = NULL;
    obj.i64_dyn_len = 0;
    PreciseIntArrays_clear(&obj);
    PreciseIntArrays_clear(&obj2);
}

// ===== Map tests =====

TEST_F(PreciseIntTest, MapMarshalUnmarshal) {
    struct PreciseIntMap obj;
    PreciseIntMap_init(&obj);

    // We need to marshal and then unmarshal from JSON
    const char* json_str =
        "{\"i32_map\":{\"a\":-999,\"b\":2147483647},"
        "\"u64_map\":{\"x\":0,\"y\":18446744073709551615}}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    int r = json_unmarshal_PreciseIntMap(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.i32_map.len, 2);
    EXPECT_EQ(obj.u64_map.len, 2);

    // Round-trip: marshal back and unmarshal again
    sstr_t out = sstr_new();
    json_marshal_PreciseIntMap(&obj, out);

    struct PreciseIntMap obj2;
    PreciseIntMap_init(&obj2);
    r = json_unmarshal_PreciseIntMap(out, &obj2);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj2.i32_map.len, 2);
    EXPECT_EQ(obj2.u64_map.len, 2);

    sstr_free(input);
    sstr_free(out);
    PreciseIntMap_clear(&obj);
    PreciseIntMap_clear(&obj2);
}

TEST_F(PreciseIntTest, EmptyArrays) {
    const char* json_str =
        "{\"i8_arr\":[],\"u32_dyn\":[],\"i64_dyn\":[],\"u64_fixed\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct PreciseIntArrays obj;
    PreciseIntArrays_init(&obj);
    int r = json_unmarshal_PreciseIntArrays(input, &obj);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(obj.u32_dyn_len, 0);
    EXPECT_EQ(obj.i64_dyn_len, 0);

    sstr_free(input);
    PreciseIntArrays_clear(&obj);
}
