#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "msgpack.gen.h"
}

/* ── Helper: pack and unpack round-trip ──────────────────────────────── */

#define ROUNDTRIP_INIT(Type) \
    struct Type src, dst; \
    Type##_init(&src); \
    Type##_init(&dst); \
    sstr_t buf = sstr_new()

#define ROUNDTRIP_PACK_UNPACK(Type) \
    ASSERT_EQ(0, msgpack_pack_##Type(&src, buf)); \
    ASSERT_GT(sstr_length(buf), (size_t)0); \
    ASSERT_EQ(0, msgpack_unpack_##Type( \
        (const unsigned char *)sstr_cstr(buf), sstr_length(buf), &dst))

#define ROUNDTRIP_CLEANUP(Type) \
    Type##_clear(&src); \
    Type##_clear(&dst); \
    sstr_free(buf)

/* ═══════════════════════════════════════════════════════════════════════
 * Scalar types
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackScalar, BasicValues) {
    ROUNDTRIP_INIT(Scalar);
    src.i = -42;
    src.l = 1234567890123LL;
    src.f = 3.14f;
    src.d = 2.718281828;
    src.b = 1;
    sstr_append_cstr(src.s, "hello world");

    ROUNDTRIP_PACK_UNPACK(Scalar);

    EXPECT_EQ(dst.i, -42);
    EXPECT_EQ(dst.l, 1234567890123LL);
    EXPECT_FLOAT_EQ(dst.f, 3.14f);
    EXPECT_DOUBLE_EQ(dst.d, 2.718281828);
    EXPECT_EQ(dst.b, 1);
    EXPECT_STREQ(sstr_cstr(dst.s), "hello world");

    ROUNDTRIP_CLEANUP(Scalar);
}

TEST(MsgpackScalar, ZeroValues) {
    ROUNDTRIP_INIT(Scalar);
    /* All zero-initialized by init */

    ROUNDTRIP_PACK_UNPACK(Scalar);

    EXPECT_EQ(dst.i, 0);
    EXPECT_EQ(dst.l, 0);
    EXPECT_FLOAT_EQ(dst.f, 0.0f);
    EXPECT_DOUBLE_EQ(dst.d, 0.0);
    EXPECT_EQ(dst.b, 0);
    EXPECT_STREQ(sstr_cstr(dst.s), "");

    ROUNDTRIP_CLEANUP(Scalar);
}

TEST(MsgpackScalar, NegativeValues) {
    ROUNDTRIP_INIT(Scalar);
    src.i = -2147483647;
    src.l = -9000000000000LL;
    src.f = -1.5f;
    src.d = -999999.999999;
    src.b = 0;
    sstr_append_cstr(src.s, "");

    ROUNDTRIP_PACK_UNPACK(Scalar);

    EXPECT_EQ(dst.i, -2147483647);
    EXPECT_EQ(dst.l, -9000000000000LL);
    EXPECT_FLOAT_EQ(dst.f, -1.5f);
    EXPECT_DOUBLE_EQ(dst.d, -999999.999999);
    EXPECT_EQ(dst.b, 0);
    EXPECT_STREQ(sstr_cstr(dst.s), "");

    ROUNDTRIP_CLEANUP(Scalar);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Nested structs
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackNested, RoundTrip) {
    ROUNDTRIP_INIT(Nested);
    src.id = 1;
    sstr_append_cstr(src.name, "parent");
    src.inner.i = 100;
    src.inner.l = 200;
    src.inner.f = 1.5f;
    src.inner.d = 2.5;
    src.inner.b = 1;
    sstr_append_cstr(src.inner.s, "child");

    ROUNDTRIP_PACK_UNPACK(Nested);

    EXPECT_EQ(dst.id, 1);
    EXPECT_STREQ(sstr_cstr(dst.name), "parent");
    EXPECT_EQ(dst.inner.i, 100);
    EXPECT_EQ(dst.inner.l, 200);
    EXPECT_FLOAT_EQ(dst.inner.f, 1.5f);
    EXPECT_DOUBLE_EQ(dst.inner.d, 2.5);
    EXPECT_EQ(dst.inner.b, 1);
    EXPECT_STREQ(sstr_cstr(dst.inner.s), "child");

    ROUNDTRIP_CLEANUP(Nested);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Fixed and dynamic arrays
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackArrays, FixedInts) {
    ROUNDTRIP_INIT(WithArrays);
    src.fixed_ints[0] = 10;
    src.fixed_ints[1] = 20;
    src.fixed_ints[2] = 30;

    ROUNDTRIP_PACK_UNPACK(WithArrays);

    EXPECT_EQ(dst.fixed_ints[0], 10);
    EXPECT_EQ(dst.fixed_ints[1], 20);
    EXPECT_EQ(dst.fixed_ints[2], 30);

    ROUNDTRIP_CLEANUP(WithArrays);
}

TEST(MsgpackArrays, DynStrings) {
    ROUNDTRIP_INIT(WithArrays);
    src.dyn_strings_len = 3;
    src.dyn_strings = (sstr_t *)calloc(3, sizeof(sstr_t));
    src.dyn_strings[0] = sstr_of("alpha", 5);
    src.dyn_strings[1] = sstr_of("beta", 4);
    src.dyn_strings[2] = sstr_of("gamma", 5);

    ROUNDTRIP_PACK_UNPACK(WithArrays);

    ASSERT_EQ(dst.dyn_strings_len, 3);
    EXPECT_STREQ(sstr_cstr(dst.dyn_strings[0]), "alpha");
    EXPECT_STREQ(sstr_cstr(dst.dyn_strings[1]), "beta");
    EXPECT_STREQ(sstr_cstr(dst.dyn_strings[2]), "gamma");

    ROUNDTRIP_CLEANUP(WithArrays);
}

TEST(MsgpackArrays, DynStructs) {
    ROUNDTRIP_INIT(WithArrays);
    src.dyn_structs_len = 2;
    src.dyn_structs = (struct Scalar *)calloc(2, sizeof(struct Scalar));
    Scalar_init(&src.dyn_structs[0]);
    Scalar_init(&src.dyn_structs[1]);
    src.dyn_structs[0].i = 1;
    sstr_append_cstr(src.dyn_structs[0].s, "a");
    src.dyn_structs[1].i = 2;
    sstr_append_cstr(src.dyn_structs[1].s, "b");

    ROUNDTRIP_PACK_UNPACK(WithArrays);

    ASSERT_EQ(dst.dyn_structs_len, 2);
    EXPECT_EQ(dst.dyn_structs[0].i, 1);
    EXPECT_STREQ(sstr_cstr(dst.dyn_structs[0].s), "a");
    EXPECT_EQ(dst.dyn_structs[1].i, 2);
    EXPECT_STREQ(sstr_cstr(dst.dyn_structs[1].s), "b");

    ROUNDTRIP_CLEANUP(WithArrays);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Enums
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackEnum, SingleEnum) {
    ROUNDTRIP_INIT(WithEnum);
    src.color = Color_BLUE;

    ROUNDTRIP_PACK_UNPACK(WithEnum);

    EXPECT_EQ(dst.color, Color_BLUE);

    ROUNDTRIP_CLEANUP(WithEnum);
}

TEST(MsgpackEnum, DynEnumArray) {
    ROUNDTRIP_INIT(WithEnum);
    src.color = Color_RED;
    src.colors_len = 3;
    src.colors = (int *)calloc(3, sizeof(int));
    src.colors[0] = Color_RED;
    src.colors[1] = Color_GREEN;
    src.colors[2] = Color_BLUE;

    ROUNDTRIP_PACK_UNPACK(WithEnum);

    EXPECT_EQ(dst.color, Color_RED);
    ASSERT_EQ(dst.colors_len, 3);
    EXPECT_EQ(dst.colors[0], Color_RED);
    EXPECT_EQ(dst.colors[1], Color_GREEN);
    EXPECT_EQ(dst.colors[2], Color_BLUE);

    ROUNDTRIP_CLEANUP(WithEnum);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Maps
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackMap, IntMap) {
    ROUNDTRIP_INIT(WithMap);
    src.scores.len = 2;
    src.scores.entries = (struct map_entry_int *)calloc(2, sizeof(struct map_entry_int));
    src.scores.entries[0].key = sstr_of("alice", 5);
    src.scores.entries[0].value = 95;
    src.scores.entries[1].key = sstr_of("bob", 3);
    src.scores.entries[1].value = 87;

    ROUNDTRIP_PACK_UNPACK(WithMap);

    ASSERT_EQ(dst.scores.len, 2);
    EXPECT_STREQ(sstr_cstr(dst.scores.entries[0].key), "alice");
    EXPECT_EQ(dst.scores.entries[0].value, 95);
    EXPECT_STREQ(sstr_cstr(dst.scores.entries[1].key), "bob");
    EXPECT_EQ(dst.scores.entries[1].value, 87);

    ROUNDTRIP_CLEANUP(WithMap);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Optional fields
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackOptional, PresentFields) {
    ROUNDTRIP_INIT(WithOptional);
    src.id = 1;
    src.has_name = true;
    sstr_append_cstr(src.name, "test");
    src.has_score = true;
    src.score = 99;

    ROUNDTRIP_PACK_UNPACK(WithOptional);

    EXPECT_EQ(dst.id, 1);
    EXPECT_TRUE(dst.has_name);
    EXPECT_STREQ(sstr_cstr(dst.name), "test");
    EXPECT_TRUE(dst.has_score);
    EXPECT_EQ(dst.score, 99);

    ROUNDTRIP_CLEANUP(WithOptional);
}

TEST(MsgpackOptional, AbsentFields) {
    ROUNDTRIP_INIT(WithOptional);
    src.id = 42;
    src.has_name = false;
    src.has_score = false;

    ROUNDTRIP_PACK_UNPACK(WithOptional);

    EXPECT_EQ(dst.id, 42);
    EXPECT_FALSE(dst.has_name);
    EXPECT_FALSE(dst.has_score);

    ROUNDTRIP_CLEANUP(WithOptional);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Nullable fields
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackNullable, NullValues) {
    ROUNDTRIP_INIT(WithNullable);
    src.id = 1;
    src.has_name = false;  /* null */
    src.has_value = false; /* null */

    ROUNDTRIP_PACK_UNPACK(WithNullable);

    EXPECT_EQ(dst.id, 1);
    EXPECT_FALSE(dst.has_name);
    EXPECT_FALSE(dst.has_value);

    ROUNDTRIP_CLEANUP(WithNullable);
}

TEST(MsgpackNullable, NonNullValues) {
    ROUNDTRIP_INIT(WithNullable);
    src.id = 2;
    src.has_name = true;
    sstr_append_cstr(src.name, "present");
    src.has_value = true;
    src.value = 42;

    ROUNDTRIP_PACK_UNPACK(WithNullable);

    EXPECT_EQ(dst.id, 2);
    EXPECT_TRUE(dst.has_name);
    EXPECT_STREQ(sstr_cstr(dst.name), "present");
    EXPECT_TRUE(dst.has_value);
    EXPECT_EQ(dst.value, 42);

    ROUNDTRIP_CLEANUP(WithNullable);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Precise-width integers
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackPrecise, AllWidths) {
    ROUNDTRIP_INIT(WithPrecise);
    src.i8  = -128;
    src.i16 = -32768;
    src.i32 = -2147483647;
    src.i64 = -9000000000000LL;
    src.u8  = 255;
    src.u16 = 65535;
    src.u32 = 4294967295U;
    src.u64 = 18446744073709551615ULL;

    ROUNDTRIP_PACK_UNPACK(WithPrecise);

    EXPECT_EQ(dst.i8,  -128);
    EXPECT_EQ(dst.i16, -32768);
    EXPECT_EQ(dst.i32, -2147483647);
    EXPECT_EQ(dst.i64, -9000000000000LL);
    EXPECT_EQ(dst.u8,  255);
    EXPECT_EQ(dst.u16, 65535);
    EXPECT_EQ(dst.u32, 4294967295U);
    EXPECT_EQ(dst.u64, 18446744073709551615ULL);

    ROUNDTRIP_CLEANUP(WithPrecise);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Field aliases (@json)
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackAlias, RoundTrip) {
    ROUNDTRIP_INIT(WithAlias);
    sstr_append_cstr(src.username, "alice");
    src.created = 1700000000;
    src.id = 42;

    ROUNDTRIP_PACK_UNPACK(WithAlias);

    EXPECT_STREQ(sstr_cstr(dst.username), "alice");
    EXPECT_EQ(dst.created, 1700000000);
    EXPECT_EQ(dst.id, 42);

    ROUNDTRIP_CLEANUP(WithAlias);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Default values
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackDefaults, InitValues) {
    struct WithDefaults d;
    WithDefaults_init(&d);

    EXPECT_EQ(d.count, 42);
    EXPECT_STREQ(sstr_cstr(d.label), "hello");
    EXPECT_EQ(d.active, 1);  /* true */
    EXPECT_EQ(d.color, Color_GREEN);

    WithDefaults_clear(&d);
}

TEST(MsgpackDefaults, RoundTrip) {
    ROUNDTRIP_INIT(WithDefaults);
    /* Use defaults */

    ROUNDTRIP_PACK_UNPACK(WithDefaults);

    EXPECT_EQ(dst.count, 42);
    EXPECT_STREQ(sstr_cstr(dst.label), "hello");
    EXPECT_EQ(dst.active, 1);
    EXPECT_EQ(dst.color, Color_GREEN);

    ROUNDTRIP_CLEANUP(WithDefaults);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Oneof (tagged union)
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackOneof, CircleRoundTrip) {
    ROUNDTRIP_INIT(Drawing);
    sstr_append_cstr(src.name, "my circle");
    src.shape.tag = Shape_circle;
    Circle_init(&src.shape.value.circle);
    src.shape.value.circle.radius = 5.0f;

    ROUNDTRIP_PACK_UNPACK(Drawing);

    EXPECT_STREQ(sstr_cstr(dst.name), "my circle");
    EXPECT_EQ(dst.shape.tag, Shape_circle);
    EXPECT_FLOAT_EQ(dst.shape.value.circle.radius, 5.0f);

    ROUNDTRIP_CLEANUP(Drawing);
}

TEST(MsgpackOneof, RectangleRoundTrip) {
    ROUNDTRIP_INIT(Drawing);
    sstr_append_cstr(src.name, "my rect");
    src.shape.tag = Shape_rectangle;
    Rectangle_init(&src.shape.value.rectangle);
    src.shape.value.rectangle.width = 10.0f;
    src.shape.value.rectangle.height = 20.0f;

    ROUNDTRIP_PACK_UNPACK(Drawing);

    EXPECT_STREQ(sstr_cstr(dst.name), "my rect");
    EXPECT_EQ(dst.shape.tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(dst.shape.value.rectangle.width, 10.0f);
    EXPECT_FLOAT_EQ(dst.shape.value.rectangle.height, 20.0f);

    ROUNDTRIP_CLEANUP(Drawing);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Array pack/unpack
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackArrayPack, StructArray) {
    struct Scalar items[2];
    Scalar_init(&items[0]);
    Scalar_init(&items[1]);
    items[0].i = 10;
    sstr_append_cstr(items[0].s, "first");
    items[1].i = 20;
    sstr_append_cstr(items[1].s, "second");

    sstr_t buf = sstr_new();
    ASSERT_EQ(0, msgpack_pack_array_Scalar(items, 2, buf));
    ASSERT_GT(sstr_length(buf), (size_t)0);

    struct Scalar *out = NULL;
    int count = 0;
    ASSERT_EQ(0, msgpack_unpack_array_Scalar(
        (const unsigned char *)sstr_cstr(buf), sstr_length(buf),
        &out, &count));
    ASSERT_EQ(count, 2);
    EXPECT_EQ(out[0].i, 10);
    EXPECT_STREQ(sstr_cstr(out[0].s), "first");
    EXPECT_EQ(out[1].i, 20);
    EXPECT_STREQ(sstr_cstr(out[1].s), "second");

    for (int i = 0; i < count; i++) Scalar_clear(&out[i]);
    free(out);
    Scalar_clear(&items[0]);
    Scalar_clear(&items[1]);
    sstr_free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Binary format verification
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackFormat, SmallPositiveInt) {
    /* msgpack positive fixint: 0x00-0x7f */
    ROUNDTRIP_INIT(Scalar);
    src.i = 42;

    ASSERT_EQ(0, msgpack_pack_Scalar(&src, buf));

    /* The packed bytes should contain 0x2a (42) as a fixint somewhere */
    const unsigned char *bytes = (const unsigned char *)sstr_cstr(buf);
    size_t len = sstr_length(buf);
    bool found = false;
    for (size_t j = 0; j < len; j++) {
        if (bytes[j] == 0x2a) { found = true; break; }
    }
    EXPECT_TRUE(found) << "Expected fixint 0x2a (42) in packed bytes";

    ROUNDTRIP_CLEANUP(Scalar);
}

TEST(MsgpackFormat, NilEncoding) {
    /* msgpack nil: 0xc0 */
    ROUNDTRIP_INIT(WithNullable);
    src.id = 1;
    src.has_name = false;
    src.has_value = false;

    ASSERT_EQ(0, msgpack_pack_WithNullable(&src, buf));

    const unsigned char *bytes = (const unsigned char *)sstr_cstr(buf);
    size_t len = sstr_length(buf);
    int nil_count = 0;
    for (size_t j = 0; j < len; j++) {
        if (bytes[j] == 0xc0) nil_count++;
    }
    EXPECT_GE(nil_count, 2) << "Expected at least 2 nil bytes for null name and value";

    ROUNDTRIP_CLEANUP(WithNullable);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Edge cases
 * ═══════════════════════════════════════════════════════════════════════ */

TEST(MsgpackEdge, EmptyDynArray) {
    ROUNDTRIP_INIT(WithArrays);
    /* No dynamic array entries, just fixed_ints default */

    ROUNDTRIP_PACK_UNPACK(WithArrays);

    EXPECT_EQ(dst.dyn_strings_len, 0);
    EXPECT_EQ(dst.dyn_structs_len, 0);

    ROUNDTRIP_CLEANUP(WithArrays);
}

TEST(MsgpackEdge, LongString) {
    ROUNDTRIP_INIT(Scalar);
    /* Create a 1000-char string */
    for (int j = 0; j < 100; j++) {
        sstr_append_cstr(src.s, "0123456789");
    }

    ROUNDTRIP_PACK_UNPACK(Scalar);

    EXPECT_EQ(sstr_length(dst.s), (size_t)1000);
    EXPECT_EQ(memcmp(sstr_cstr(src.s), sstr_cstr(dst.s), 1000), 0);

    ROUNDTRIP_CLEANUP(Scalar);
}

TEST(MsgpackEdge, EmptyMapRoundTrip) {
    ROUNDTRIP_INIT(WithMap);
    /* All maps empty by default */

    ROUNDTRIP_PACK_UNPACK(WithMap);

    EXPECT_EQ(dst.scores.len, 0);
    EXPECT_EQ(dst.labels.len, 0);
    EXPECT_EQ(dst.structs.len, 0);

    ROUNDTRIP_CLEANUP(WithMap);
}

TEST(MsgpackEdge, InvalidData) {
    struct Scalar s;
    Scalar_init(&s);

    /* Completely invalid data */
    unsigned char bad[] = {0xff, 0xff, 0xff};
    int r = msgpack_unpack_Scalar(bad, 3, &s);
    EXPECT_NE(r, 0) << "Should fail on invalid data";

    /* Empty data */
    r = msgpack_unpack_Scalar(NULL, 0, &s);
    EXPECT_NE(r, 0) << "Should fail on empty data";

    Scalar_clear(&s);
}
