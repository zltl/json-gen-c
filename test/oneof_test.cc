extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>
#include <cstring>

class OneofTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Standalone Shape marshal tests
// ============================================================================

TEST_F(OneofTest, MarshalCircle) {
    struct Shape s;
    Shape_init(&s);
    s.tag = Shape_circle;
    s.value.circle.radius = 5.0f;

    sstr_t out = sstr_new();
    json_marshal_Shape(&s, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"type\":\"circle\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"radius\":5") != NULL) << "json: " << json;

    sstr_free(out);
    Shape_clear(&s);
}

TEST_F(OneofTest, MarshalRectangle) {
    struct Shape s;
    Shape_init(&s);
    s.tag = Shape_rectangle;
    s.value.rectangle.width = 10.0f;
    s.value.rectangle.height = 20.0f;

    sstr_t out = sstr_new();
    json_marshal_Shape(&s, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"type\":\"rectangle\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"width\":10") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"height\":20") != NULL) << "json: " << json;

    sstr_free(out);
    Shape_clear(&s);
}

TEST_F(OneofTest, MarshalTriangle) {
    struct Shape s;
    Shape_init(&s);
    s.tag = Shape_triangle;
    s.value.triangle.base = 3.0f;
    s.value.triangle.height = 4.0f;
    s.value.triangle.label = sstr_of("tri1", 4);

    sstr_t out = sstr_new();
    json_marshal_Shape(&s, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"type\":\"triangle\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"base\":3") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"height\":4") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"label\":\"tri1\"") != NULL) << "json: " << json;

    sstr_free(out);
    Shape_clear(&s);
}

// ============================================================================
// Standalone Shape unmarshal tests
// ============================================================================

TEST_F(OneofTest, UnmarshalCircle) {
    const char* json_str = "{\"type\":\"circle\",\"radius\":5.0}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_EQ(r, 0) << "unmarshal failed";
    EXPECT_EQ(s.tag, Shape_circle);
    EXPECT_FLOAT_EQ(s.value.circle.radius, 5.0f);

    sstr_free(input);
    Shape_clear(&s);
}

TEST_F(OneofTest, UnmarshalRectangle) {
    const char* json_str = "{\"type\":\"rectangle\",\"width\":10.0,\"height\":20.0}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_EQ(r, 0) << "unmarshal failed";
    EXPECT_EQ(s.tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(s.value.rectangle.width, 10.0f);
    EXPECT_FLOAT_EQ(s.value.rectangle.height, 20.0f);

    sstr_free(input);
    Shape_clear(&s);
}

TEST_F(OneofTest, UnmarshalTriangle) {
    const char* json_str = "{\"type\":\"triangle\",\"base\":3.0,\"height\":4.0,\"label\":\"tri1\"}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_EQ(r, 0) << "unmarshal failed";
    EXPECT_EQ(s.tag, Shape_triangle);
    EXPECT_FLOAT_EQ(s.value.triangle.base, 3.0f);
    EXPECT_FLOAT_EQ(s.value.triangle.height, 4.0f);
    EXPECT_STREQ(sstr_cstr(s.value.triangle.label), "tri1");

    sstr_free(input);
    Shape_clear(&s);
}

// Tag field NOT first — tests two-pass scan
TEST_F(OneofTest, UnmarshalTagNotFirst) {
    const char* json_str = "{\"radius\":7.5,\"type\":\"circle\"}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_EQ(r, 0) << "unmarshal with tag-not-first failed";
    EXPECT_EQ(s.tag, Shape_circle);
    EXPECT_FLOAT_EQ(s.value.circle.radius, 7.5f);

    sstr_free(input);
    Shape_clear(&s);
}

TEST_F(OneofTest, UnmarshalTagMiddle) {
    const char* json_str = "{\"width\":15.0,\"type\":\"rectangle\",\"height\":25.0}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_EQ(r, 0) << "unmarshal with tag in middle failed";
    EXPECT_EQ(s.tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(s.value.rectangle.width, 15.0f);
    EXPECT_FLOAT_EQ(s.value.rectangle.height, 25.0f);

    sstr_free(input);
    Shape_clear(&s);
}

// ============================================================================
// Round-trip tests
// ============================================================================

TEST_F(OneofTest, RoundTripCircle) {
    struct Shape orig;
    Shape_init(&orig);
    orig.tag = Shape_circle;
    orig.value.circle.radius = 12.5f;

    sstr_t json = sstr_new();
    json_marshal_Shape(&orig, json);

    struct Shape decoded;
    Shape_init(&decoded);
    int r = json_unmarshal_Shape(json, &decoded);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(decoded.tag, Shape_circle);
    EXPECT_FLOAT_EQ(decoded.value.circle.radius, 12.5f);

    sstr_free(json);
    Shape_clear(&orig);
    Shape_clear(&decoded);
}

TEST_F(OneofTest, RoundTripTriangle) {
    struct Shape orig;
    Shape_init(&orig);
    orig.tag = Shape_triangle;
    orig.value.triangle.base = 6.0f;
    orig.value.triangle.height = 8.0f;
    orig.value.triangle.label = sstr_of("test_label", 10);

    sstr_t json = sstr_new();
    json_marshal_Shape(&orig, json);

    struct Shape decoded;
    Shape_init(&decoded);
    int r = json_unmarshal_Shape(json, &decoded);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(decoded.tag, Shape_triangle);
    EXPECT_FLOAT_EQ(decoded.value.triangle.base, 6.0f);
    EXPECT_FLOAT_EQ(decoded.value.triangle.height, 8.0f);
    EXPECT_STREQ(sstr_cstr(decoded.value.triangle.label), "test_label");

    sstr_free(json);
    Shape_clear(&orig);
    Shape_clear(&decoded);
}

// ============================================================================
// Error handling
// ============================================================================

TEST_F(OneofTest, UnmarshalUnknownTag) {
    const char* json_str = "{\"type\":\"hexagon\",\"sides\":6}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_NE(r, 0) << "expected error for unknown tag";

    sstr_free(input);
    Shape_clear(&s);
}

TEST_F(OneofTest, UnmarshalMissingTag) {
    const char* json_str = "{\"radius\":5.0}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_NE(r, 0) << "expected error for missing tag";

    sstr_free(input);
    Shape_clear(&s);
}

TEST_F(OneofTest, UnmarshalEmptyObject) {
    const char* json_str = "{}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape s;
    Shape_init(&s);
    int r = json_unmarshal_Shape(input, &s);
    EXPECT_NE(r, 0) << "expected error for empty object";

    sstr_free(input);
    Shape_clear(&s);
}

// ============================================================================
// Array of oneof (standalone)
// ============================================================================

TEST_F(OneofTest, MarshalArray) {
    struct Shape shapes[2];
    Shape_init(&shapes[0]);
    Shape_init(&shapes[1]);
    shapes[0].tag = Shape_circle;
    shapes[0].value.circle.radius = 1.0f;
    shapes[1].tag = Shape_rectangle;
    shapes[1].value.rectangle.width = 2.0f;
    shapes[1].value.rectangle.height = 3.0f;

    sstr_t out = sstr_new();
    json_marshal_array_Shape(shapes, 2, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"circle\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"rectangle\"") != NULL) << "json: " << json;

    sstr_free(out);
    Shape_clear(&shapes[0]);
    Shape_clear(&shapes[1]);
}

TEST_F(OneofTest, UnmarshalArray) {
    const char* json_str = "[{\"type\":\"circle\",\"radius\":1.0},{\"type\":\"rectangle\",\"width\":2.0,\"height\":3.0}]";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Shape* shapes = NULL;
    int len = 0;
    int r = json_unmarshal_array_Shape(input, &shapes, &len);
    EXPECT_EQ(r, 0) << "array unmarshal failed";
    EXPECT_EQ(len, 2);

    EXPECT_EQ(shapes[0].tag, Shape_circle);
    EXPECT_FLOAT_EQ(shapes[0].value.circle.radius, 1.0f);
    EXPECT_EQ(shapes[1].tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(shapes[1].value.rectangle.width, 2.0f);
    EXPECT_FLOAT_EQ(shapes[1].value.rectangle.height, 3.0f);

    for (int i = 0; i < len; i++) Shape_clear(&shapes[i]);
    free(shapes);
    sstr_free(input);
}

TEST_F(OneofTest, RoundTripArray) {
    struct Shape shapes[3];
    Shape_init(&shapes[0]);
    Shape_init(&shapes[1]);
    Shape_init(&shapes[2]);
    shapes[0].tag = Shape_circle;
    shapes[0].value.circle.radius = 10.0f;
    shapes[1].tag = Shape_rectangle;
    shapes[1].value.rectangle.width = 20.0f;
    shapes[1].value.rectangle.height = 30.0f;
    shapes[2].tag = Shape_triangle;
    shapes[2].value.triangle.base = 5.0f;
    shapes[2].value.triangle.height = 7.0f;
    shapes[2].value.triangle.label = sstr_of("t", 1);

    sstr_t json = sstr_new();
    json_marshal_array_Shape(shapes, 3, json);

    struct Shape* decoded = NULL;
    int len = 0;
    int r = json_unmarshal_array_Shape(json, &decoded, &len);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(len, 3);
    EXPECT_EQ(decoded[0].tag, Shape_circle);
    EXPECT_FLOAT_EQ(decoded[0].value.circle.radius, 10.0f);
    EXPECT_EQ(decoded[1].tag, Shape_rectangle);
    EXPECT_EQ(decoded[2].tag, Shape_triangle);
    EXPECT_STREQ(sstr_cstr(decoded[2].value.triangle.label), "t");

    for (int i = 0; i < len; i++) Shape_clear(&decoded[i]);
    free(decoded);
    sstr_free(json);
    Shape_clear(&shapes[0]);
    Shape_clear(&shapes[1]);
    Shape_clear(&shapes[2]);
}

// ============================================================================
// Oneof in struct (Drawing) — uses runtime dispatch
// ============================================================================

TEST_F(OneofTest, DrawingMarshalWithShape) {
    struct Drawing d;
    Drawing_init(&d);
    d.name = sstr_of("my_drawing", 10);
    d.shape.tag = Shape_circle;
    d.shape.value.circle.radius = 9.0f;

    sstr_t out = sstr_new();
    json_marshal_Drawing(&d, out);
    const char* json = sstr_cstr(out);

    EXPECT_TRUE(strstr(json, "\"my_drawing\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"type\":\"circle\"") != NULL) << "json: " << json;
    EXPECT_TRUE(strstr(json, "\"radius\":9") != NULL) << "json: " << json;

    sstr_free(out);
    Drawing_clear(&d);
}

TEST_F(OneofTest, DrawingUnmarshalWithShape) {
    const char* json_str = "{\"name\":\"test\",\"shape\":{\"type\":\"rectangle\",\"width\":4.0,\"height\":5.0},\"shapes\":[]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Drawing d;
    Drawing_init(&d);
    int r = json_unmarshal_Drawing(input, &d);
    EXPECT_EQ(r, 0) << "Drawing unmarshal failed";
    EXPECT_STREQ(sstr_cstr(d.name), "test");
    EXPECT_EQ(d.shape.tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(d.shape.value.rectangle.width, 4.0f);
    EXPECT_FLOAT_EQ(d.shape.value.rectangle.height, 5.0f);

    sstr_free(input);
    Drawing_clear(&d);
}

TEST_F(OneofTest, DrawingUnmarshalWithShapeArray) {
    const char* json_str =
        "{\"name\":\"multi\",\"shape\":{\"type\":\"circle\",\"radius\":1.0},"
        "\"shapes\":[{\"type\":\"circle\",\"radius\":2.0},{\"type\":\"triangle\",\"base\":3.0,\"height\":4.0,\"label\":\"x\"}]}";
    sstr_t input = sstr_of(json_str, strlen(json_str));

    struct Drawing d;
    Drawing_init(&d);
    int r = json_unmarshal_Drawing(input, &d);
    EXPECT_EQ(r, 0) << "Drawing unmarshal with array failed";
    EXPECT_STREQ(sstr_cstr(d.name), "multi");
    EXPECT_EQ(d.shape.tag, Shape_circle);
    EXPECT_FLOAT_EQ(d.shape.value.circle.radius, 1.0f);
    EXPECT_EQ(d.shapes_len, 2);
    EXPECT_EQ(d.shapes[0].tag, Shape_circle);
    EXPECT_FLOAT_EQ(d.shapes[0].value.circle.radius, 2.0f);
    EXPECT_EQ(d.shapes[1].tag, Shape_triangle);
    EXPECT_FLOAT_EQ(d.shapes[1].value.triangle.base, 3.0f);
    EXPECT_STREQ(sstr_cstr(d.shapes[1].value.triangle.label), "x");

    sstr_free(input);
    Drawing_clear(&d);
}

TEST_F(OneofTest, DrawingRoundTrip) {
    struct Drawing d;
    Drawing_init(&d);
    d.name = sstr_of("roundtrip_draw", 14);
    d.shape.tag = Shape_triangle;
    d.shape.value.triangle.base = 11.0f;
    d.shape.value.triangle.height = 22.0f;
    d.shape.value.triangle.label = sstr_of("main", 4);

    // Add array of shapes
    struct Shape arr[2];
    Shape_init(&arr[0]);
    Shape_init(&arr[1]);
    arr[0].tag = Shape_circle;
    arr[0].value.circle.radius = 100.0f;
    arr[1].tag = Shape_rectangle;
    arr[1].value.rectangle.width = 50.0f;
    arr[1].value.rectangle.height = 60.0f;
    d.shapes = arr;
    d.shapes_len = 2;

    sstr_t json = sstr_new();
    json_marshal_Drawing(&d, json);

    struct Drawing decoded;
    Drawing_init(&decoded);
    int r = json_unmarshal_Drawing(json, &decoded);
    EXPECT_EQ(r, 0) << "Drawing round-trip unmarshal failed, json: " << sstr_cstr(json);
    EXPECT_STREQ(sstr_cstr(decoded.name), "roundtrip_draw");
    EXPECT_EQ(decoded.shape.tag, Shape_triangle);
    EXPECT_FLOAT_EQ(decoded.shape.value.triangle.base, 11.0f);
    EXPECT_FLOAT_EQ(decoded.shape.value.triangle.height, 22.0f);
    EXPECT_STREQ(sstr_cstr(decoded.shape.value.triangle.label), "main");
    EXPECT_EQ(decoded.shapes_len, 2);
    EXPECT_EQ(decoded.shapes[0].tag, Shape_circle);
    EXPECT_FLOAT_EQ(decoded.shapes[0].value.circle.radius, 100.0f);
    EXPECT_EQ(decoded.shapes[1].tag, Shape_rectangle);
    EXPECT_FLOAT_EQ(decoded.shapes[1].value.rectangle.width, 50.0f);

    sstr_free(json);
    // Don't clear arr since it's stack-allocated; null out before Drawing_clear
    d.shapes = NULL;
    d.shapes_len = 0;
    Drawing_clear(&d);
    Drawing_clear(&decoded);
    Shape_clear(&arr[0]);
    Shape_clear(&arr[1]);
}

// ============================================================================
// Init / Clear
// ============================================================================

TEST_F(OneofTest, InitZeroes) {
    struct Shape s;
    Shape_init(&s);
    EXPECT_EQ((int)s.tag, 0);
    // The first variant (circle) should be zero-init
    EXPECT_FLOAT_EQ(s.value.circle.radius, 0.0f);
    Shape_clear(&s);
}

TEST_F(OneofTest, ClearWithStringVariant) {
    struct Shape s;
    Shape_init(&s);
    s.tag = Shape_triangle;
    s.value.triangle.label = sstr_of("allocated_label", 15);
    // Should free the label string without leaks
    Shape_clear(&s);
}
