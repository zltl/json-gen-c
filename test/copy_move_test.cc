extern "C" {
#include "json.gen.h"
}

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>

static void fill_complex(struct ComplexStruct* obj) {
    obj->simple_int = 7;
    obj->simple_long = 9001;
    obj->simple_float = 1.25f;
    obj->simple_double = 2.5;
    obj->simple_bool = 1;
    obj->simple_string = sstr("complex");

    obj->int_array_len = 3;
    obj->int_array = static_cast<int*>(std::malloc(sizeof(int) * 3));
    obj->int_array[0] = 1;
    obj->int_array[1] = 2;
    obj->int_array[2] = 3;

    obj->string_array_len = 2;
    obj->string_array = static_cast<sstr_t*>(std::calloc(2, sizeof(sstr_t)));
    obj->string_array[0] = sstr("first");
    obj->string_array[1] = sstr("second");

    obj->address.number = sstr("42");
    obj->address.street = sstr("Main");

    obj->contacts_len = 1;
    obj->contacts = static_cast<struct Person*>(std::calloc(1, sizeof(struct Person)));
    Person_init(&obj->contacts[0]);
    obj->contacts[0].name = sstr("Alice");
    obj->contacts[0].age = sstr("30");
}

static void fill_drawing(struct Drawing* obj) {
    obj->name = sstr("drawing");

    obj->shape.tag = Shape_triangle;
    obj->shape.value.triangle.base = 3.0f;
    obj->shape.value.triangle.height = 4.0f;
    obj->shape.value.triangle.label = sstr("primary");

    obj->shapes_len = 2;
    obj->shapes = static_cast<struct Shape*>(std::calloc(2, sizeof(struct Shape)));
    Shape_init(&obj->shapes[0]);
    Shape_init(&obj->shapes[1]);
    obj->shapes[0].tag = Shape_circle;
    obj->shapes[0].value.circle.radius = 5.0f;
    obj->shapes[1].tag = Shape_triangle;
    obj->shapes[1].value.triangle.base = 6.0f;
    obj->shapes[1].value.triangle.height = 8.0f;
    obj->shapes[1].value.triangle.label = sstr("array-triangle");
}

TEST(CopyMoveTest, RejectsNullArguments) {
    struct TestStruct src;
    TestStruct_init(&src);

    EXPECT_EQ(TestStruct_copy(nullptr, &src), -1);
    EXPECT_EQ(TestStruct_copy(&src, nullptr), -1);
    EXPECT_EQ(TestStruct_move(nullptr, &src), -1);
    EXPECT_EQ(TestStruct_move(&src, nullptr), -1);

    TestStruct_clear(&src);
}

TEST(CopyMoveTest, StringCopyIsIndependent) {
    struct TestStruct src;
    struct TestStruct dest;
    TestStruct_init(&src);
    TestStruct_init(&dest);

    src.sstr_val = sstr("alpha");
    dest.sstr_val = sstr("old");

    ASSERT_EQ(TestStruct_copy(&dest, &src), 0);
    ASSERT_NE(dest.sstr_val, nullptr);
    ASSERT_NE(src.sstr_val, nullptr);
    EXPECT_NE(dest.sstr_val, src.sstr_val);
    EXPECT_STREQ(sstr_cstr(dest.sstr_val), "alpha");

    sstr_clear(src.sstr_val);
    sstr_append_cstr(src.sstr_val, "changed");
    EXPECT_STREQ(sstr_cstr(dest.sstr_val), "alpha");

    TestStruct_clear(&src);
    TestStruct_clear(&dest);
}

TEST(CopyMoveTest, DynamicArraysAndNestedStructsCopyDeeply) {
    struct ComplexStruct src;
    struct ComplexStruct dest;
    ComplexStruct_init(&src);
    ComplexStruct_init(&dest);
    fill_complex(&src);
    fill_complex(&dest);

    ASSERT_EQ(ComplexStruct_copy(&dest, &src), 0);

    ASSERT_NE(dest.simple_string, nullptr);
    ASSERT_NE(src.simple_string, nullptr);
    EXPECT_NE(dest.simple_string, src.simple_string);
    EXPECT_STREQ(sstr_cstr(dest.simple_string), "complex");

    ASSERT_EQ(dest.int_array_len, 3);
    ASSERT_NE(dest.int_array, nullptr);
    EXPECT_NE(dest.int_array, src.int_array);
    EXPECT_EQ(dest.int_array[0], 1);

    ASSERT_EQ(dest.string_array_len, 2);
    ASSERT_NE(dest.string_array, nullptr);
    EXPECT_NE(dest.string_array, src.string_array);
    EXPECT_NE(dest.string_array[0], src.string_array[0]);
    EXPECT_STREQ(sstr_cstr(dest.string_array[0]), "first");

    EXPECT_NE(dest.address.number, src.address.number);
    EXPECT_STREQ(sstr_cstr(dest.address.number), "42");

    ASSERT_EQ(dest.contacts_len, 1);
    ASSERT_NE(dest.contacts, nullptr);
    EXPECT_NE(dest.contacts, src.contacts);
    EXPECT_NE(dest.contacts[0].name, src.contacts[0].name);
    EXPECT_STREQ(sstr_cstr(dest.contacts[0].name), "Alice");

    src.int_array[0] = 99;
    sstr_clear(src.string_array[0]);
    sstr_append_cstr(src.string_array[0], "mutated");
    sstr_clear(src.address.number);
    sstr_append_cstr(src.address.number, "99");
    sstr_clear(src.contacts[0].name);
    sstr_append_cstr(src.contacts[0].name, "Bob");

    EXPECT_EQ(dest.int_array[0], 1);
    EXPECT_STREQ(sstr_cstr(dest.string_array[0]), "first");
    EXPECT_STREQ(sstr_cstr(dest.address.number), "42");
    EXPECT_STREQ(sstr_cstr(dest.contacts[0].name), "Alice");

    ComplexStruct_clear(&src);
    ComplexStruct_clear(&dest);
}

TEST(CopyMoveTest, MapsCopyKeysAndValuesDeeply) {
    sstr_t json = sstr("{\"int_map\":{\"a\":42},"
                       "\"long_map\":{\"b\":9},"
                       "\"float_map\":{\"c\":1.5},"
                       "\"double_map\":{\"d\":2.5},"
                       "\"bool_map\":{\"e\":true},"
                       "\"str_map\":{\"g\":\"hello\"},"
                       "\"enum_map\":{\"h\":\"GREEN\"},"
                       "\"struct_map\":{\"i\":{\"name\":\"John\",\"age\":\"30\"}}}");

    struct MapAllTypesStruct src;
    struct MapAllTypesStruct dest;
    MapAllTypesStruct_init(&src);
    MapAllTypesStruct_init(&dest);
    ASSERT_EQ(json_unmarshal_MapAllTypesStruct(json, &src), 0);

    ASSERT_EQ(MapAllTypesStruct_copy(&dest, &src), 0);
    ASSERT_EQ(dest.str_map.len, 1);
    ASSERT_EQ(dest.struct_map.len, 1);
    EXPECT_NE(dest.str_map.entries, src.str_map.entries);
    EXPECT_NE(dest.str_map.entries[0].key, src.str_map.entries[0].key);
    EXPECT_NE(dest.str_map.entries[0].value, src.str_map.entries[0].value);
    EXPECT_STREQ(sstr_cstr(dest.str_map.entries[0].value), "hello");
    EXPECT_NE(dest.struct_map.entries[0].key, src.struct_map.entries[0].key);
    EXPECT_NE(dest.struct_map.entries[0].value.name, src.struct_map.entries[0].value.name);
    EXPECT_STREQ(sstr_cstr(dest.struct_map.entries[0].value.name), "John");

    sstr_clear(src.str_map.entries[0].value);
    sstr_append_cstr(src.str_map.entries[0].value, "changed");
    sstr_clear(src.struct_map.entries[0].value.name);
    sstr_append_cstr(src.struct_map.entries[0].value.name, "Jane");
    EXPECT_STREQ(sstr_cstr(dest.str_map.entries[0].value), "hello");
    EXPECT_STREQ(sstr_cstr(dest.struct_map.entries[0].value.name), "John");

    sstr_free(json);
    MapAllTypesStruct_clear(&src);
    MapAllTypesStruct_clear(&dest);
}

TEST(CopyMoveTest, OneofCopyAndMovePreservePayload) {
    struct Shape src;
    struct Shape copy;
    struct Shape moved;
    Shape_init(&src);
    Shape_init(&copy);
    Shape_init(&moved);

    src.tag = Shape_triangle;
    src.value.triangle.base = 3.0f;
    src.value.triangle.height = 4.0f;
    src.value.triangle.label = sstr("tri");

    ASSERT_EQ(Shape_copy(&copy, &src), 0);
    EXPECT_EQ(copy.tag, Shape_triangle);
    EXPECT_NE(copy.value.triangle.label, src.value.triangle.label);
    EXPECT_STREQ(sstr_cstr(copy.value.triangle.label), "tri");

    sstr_clear(src.value.triangle.label);
    sstr_append_cstr(src.value.triangle.label, "moved-tri");

    sstr_t moved_label = src.value.triangle.label;
    ASSERT_EQ(Shape_move(&moved, &src), 0);
    EXPECT_EQ(moved.tag, Shape_triangle);
    EXPECT_EQ(moved.value.triangle.label, moved_label);
    EXPECT_STREQ(sstr_cstr(moved.value.triangle.label), "moved-tri");
    EXPECT_EQ(src.value.triangle.label, nullptr);

    Shape_clear(&src);
    Shape_clear(&copy);
    Shape_clear(&moved);
}

TEST(CopyMoveTest, StructWithOneofArrayCopyAndMove) {
    struct Drawing src;
    struct Drawing copy;
    struct Drawing moved;
    Drawing_init(&src);
    Drawing_init(&copy);
    Drawing_init(&moved);
    fill_drawing(&src);

    ASSERT_EQ(Drawing_copy(&copy, &src), 0);
    EXPECT_NE(copy.name, src.name);
    EXPECT_STREQ(sstr_cstr(copy.name), "drawing");
    EXPECT_EQ(copy.shape.tag, Shape_triangle);
    EXPECT_NE(copy.shape.value.triangle.label, src.shape.value.triangle.label);
    ASSERT_EQ(copy.shapes_len, 2);
    ASSERT_NE(copy.shapes, nullptr);
    EXPECT_NE(copy.shapes, src.shapes);
    EXPECT_EQ(copy.shapes[1].tag, Shape_triangle);
    EXPECT_NE(copy.shapes[1].value.triangle.label,
              src.shapes[1].value.triangle.label);
    EXPECT_STREQ(sstr_cstr(copy.shapes[1].value.triangle.label), "array-triangle");

    struct Shape* old_shapes = src.shapes;
    sstr_t old_name = src.name;
    ASSERT_EQ(Drawing_move(&moved, &src), 0);
    EXPECT_EQ(moved.name, old_name);
    EXPECT_EQ(moved.shapes, old_shapes);
    EXPECT_EQ(moved.shapes_len, 2);
    EXPECT_EQ(src.name, nullptr);
    EXPECT_EQ(src.shapes, nullptr);
    EXPECT_EQ(src.shapes_len, 0);

    Drawing_clear(&src);
    Drawing_clear(&copy);
    Drawing_clear(&moved);
}

TEST(CopyMoveTest, OptionalNullablePresenceIsCopied) {
    struct OptionalNullableStruct src;
    struct OptionalNullableStruct dest;
    OptionalNullableStruct_init(&src);
    OptionalNullableStruct_init(&dest);

    src.id = 10;
    src.name = sstr("present");
    src.has_name = true;
    src.score = 1234;
    src.has_score = false;

    dest.name = sstr("old");
    dest.has_name = true;
    dest.score = 55;
    dest.has_score = true;

    ASSERT_EQ(OptionalNullableStruct_copy(&dest, &src), 0);
    EXPECT_EQ(dest.id, 10);
    EXPECT_TRUE(dest.has_name);
    ASSERT_NE(dest.name, nullptr);
    EXPECT_NE(dest.name, src.name);
    EXPECT_STREQ(sstr_cstr(dest.name), "present");
    EXPECT_FALSE(dest.has_score);
    EXPECT_EQ(dest.score, 0);

    OptionalNullableStruct_clear(&src);
    OptionalNullableStruct_clear(&dest);
}

TEST(CopyMoveTest, MoveTransfersDynamicOwnership) {
    struct ComplexStruct src;
    struct ComplexStruct dest;
    ComplexStruct_init(&src);
    ComplexStruct_init(&dest);
    fill_complex(&src);
    fill_complex(&dest);

    sstr_t old_string = src.simple_string;
    int* old_int_array = src.int_array;
    sstr_t* old_string_array = src.string_array;
    struct Person* old_contacts = src.contacts;

    ASSERT_EQ(ComplexStruct_move(&dest, &src), 0);
    EXPECT_EQ(dest.simple_string, old_string);
    EXPECT_EQ(dest.int_array, old_int_array);
    EXPECT_EQ(dest.string_array, old_string_array);
    EXPECT_EQ(dest.contacts, old_contacts);
    EXPECT_EQ(dest.int_array_len, 3);
    EXPECT_EQ(src.simple_string, nullptr);
    EXPECT_EQ(src.int_array, nullptr);
    EXPECT_EQ(src.int_array_len, 0);
    EXPECT_EQ(src.string_array, nullptr);
    EXPECT_EQ(src.string_array_len, 0);
    EXPECT_EQ(src.contacts, nullptr);
    EXPECT_EQ(src.contacts_len, 0);

    ComplexStruct_clear(&src);
    ComplexStruct_clear(&dest);
}

TEST(CopyMoveTest, SelfCopyAndSelfMoveAreNoops) {
    struct ComplexStruct obj;
    ComplexStruct_init(&obj);
    fill_complex(&obj);

    sstr_t old_string = obj.simple_string;
    int* old_int_array = obj.int_array;
    ASSERT_EQ(ComplexStruct_copy(&obj, &obj), 0);
    ASSERT_EQ(ComplexStruct_move(&obj, &obj), 0);
    EXPECT_EQ(obj.simple_string, old_string);
    EXPECT_EQ(obj.int_array, old_int_array);
    EXPECT_STREQ(sstr_cstr(obj.simple_string), "complex");
    ASSERT_EQ(obj.int_array_len, 3);
    EXPECT_EQ(obj.int_array[0], 1);

    ComplexStruct_clear(&obj);
}

static int fail_alloc_budget = -1;

static void* fail_malloc(size_t size) {
    if (fail_alloc_budget == 0) {
        return nullptr;
    }
    if (fail_alloc_budget > 0) {
        fail_alloc_budget--;
    }
    return std::malloc(size);
}

static void* fail_realloc(void* ptr, size_t size) {
    if (fail_alloc_budget == 0) {
        return nullptr;
    }
    if (fail_alloc_budget > 0) {
        fail_alloc_budget--;
    }
    return std::realloc(ptr, size);
}

static void fail_free(void* ptr) {
    std::free(ptr);
}

TEST(CopyMoveTest, CopyFailureLeavesDestinationCleared) {
    struct ComplexStruct src;
    struct ComplexStruct dest;
    ComplexStruct_init(&src);
    ComplexStruct_init(&dest);
    fill_complex(&src);
    fill_complex(&dest);

    fail_alloc_budget = 0;
    json_gen_c_set_alloc(fail_malloc, fail_realloc, fail_free);
    int rc = ComplexStruct_copy(&dest, &src);
    json_gen_c_set_alloc(nullptr, nullptr, nullptr);

    EXPECT_EQ(rc, -1);
    EXPECT_EQ(dest.simple_string, nullptr);
    EXPECT_EQ(dest.int_array, nullptr);
    EXPECT_EQ(dest.int_array_len, 0);
    EXPECT_EQ(dest.string_array, nullptr);
    EXPECT_EQ(dest.string_array_len, 0);
    EXPECT_EQ(dest.contacts, nullptr);
    EXPECT_EQ(dest.contacts_len, 0);

    ComplexStruct_clear(&src);
    ComplexStruct_clear(&dest);
}