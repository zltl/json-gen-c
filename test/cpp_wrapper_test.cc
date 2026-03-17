#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "json_gen_c.gen.hpp"

// =========================================================================
// Basic RAII
// =========================================================================

TEST(CppWrapper, DefaultConstruct) {
    jgc::TestStruct ts;
    EXPECT_EQ(ts.int_val(), 0);
    EXPECT_EQ(ts.float_val(), 0.0f);
    EXPECT_EQ(ts.sstr_val(), "");
}

TEST(CppWrapper, SetAndGetScalars) {
    jgc::TestStruct ts;
    ts.set_int_val(42);
    ts.set_long_val(999999L);
    ts.set_float_val(3.14f);
    ts.set_double_val(2.718);
    ts.set_bool_val(true);
    ts.set_sstr_val("hello");

    EXPECT_EQ(ts.int_val(), 42);
    EXPECT_EQ(ts.long_val(), 999999L);
    EXPECT_FLOAT_EQ(ts.float_val(), 3.14f);
    EXPECT_DOUBLE_EQ(ts.double_val(), 2.718);
    EXPECT_TRUE(ts.bool_val());
    EXPECT_EQ(ts.sstr_val(), "hello");
}

TEST(CppWrapper, StringSetFromCStr) {
    jgc::Triangle t;
    t.set_label("test_label");
    EXPECT_EQ(t.label(), "test_label");
}

// =========================================================================
// Marshal / Unmarshal
// =========================================================================

TEST(CppWrapper, MarshalUnmarshalRoundTrip) {
    jgc::TestStruct ts;
    ts.set_int_val(10);
    ts.set_long_val(20);
    ts.set_float_val(1.5f);
    ts.set_double_val(2.5);
    ts.set_bool_val(true);
    ts.set_sstr_val("round-trip");

    std::string json = ts.marshal();
    EXPECT_FALSE(json.empty());

    jgc::TestStruct ts2 = jgc::TestStruct::unmarshal(json);
    EXPECT_EQ(ts2.int_val(), 10);
    EXPECT_EQ(ts2.long_val(), 20);
    EXPECT_FLOAT_EQ(ts2.float_val(), 1.5f);
    EXPECT_DOUBLE_EQ(ts2.double_val(), 2.5);
    EXPECT_TRUE(ts2.bool_val());
    EXPECT_EQ(ts2.sstr_val(), "round-trip");
}

TEST(CppWrapper, UnmarshalInto) {
    jgc::Triangle t;
    std::string json = R"({"base":3.0,"height":4.0,"label":"tri"})";
    EXPECT_TRUE(t.unmarshal_into(json));
    EXPECT_FLOAT_EQ(t.base(), 3.0f);
    EXPECT_FLOAT_EQ(t.height(), 4.0f);
    EXPECT_EQ(t.label(), "tri");
}

TEST(CppWrapper, UnmarshalBadJsonThrows) {
    EXPECT_THROW(jgc::TestStruct::unmarshal("{invalid"), std::runtime_error);
}

// =========================================================================
// Move semantics
// =========================================================================

TEST(CppWrapper, MoveConstruct) {
    jgc::Triangle a;
    a.set_base(10.0f);
    a.set_label("moved");

    jgc::Triangle b(std::move(a));
    EXPECT_FLOAT_EQ(b.base(), 10.0f);
    EXPECT_EQ(b.label(), "moved");
    // a is in a moved-from state: data_ is zeroed
}

TEST(CppWrapper, MoveAssign) {
    jgc::Triangle a;
    a.set_base(5.0f);
    a.set_label("assign_me");

    jgc::Triangle b;
    b = std::move(a);
    EXPECT_FLOAT_EQ(b.base(), 5.0f);
    EXPECT_EQ(b.label(), "assign_me");
}

// =========================================================================
// Copy semantics (via marshal round-trip)
// =========================================================================

TEST(CppWrapper, CopyConstruct) {
    jgc::Triangle orig;
    orig.set_base(7.5f);
    orig.set_height(12.0f);
    orig.set_label("copy_me");

    jgc::Triangle copy(orig);
    EXPECT_FLOAT_EQ(copy.base(), 7.5f);
    EXPECT_FLOAT_EQ(copy.height(), 12.0f);
    EXPECT_EQ(copy.label(), "copy_me");

    // Ensure deep copy: modifying orig doesn't affect copy
    orig.set_label("modified");
    EXPECT_EQ(copy.label(), "copy_me");
}

TEST(CppWrapper, CopyAssign) {
    jgc::Triangle orig;
    orig.set_base(1.0f);
    orig.set_label("orig");

    jgc::Triangle dest;
    dest.set_label("will_be_overwritten");
    dest = orig;
    EXPECT_FLOAT_EQ(dest.base(), 1.0f);
    EXPECT_EQ(dest.label(), "orig");

    // Deep copy check
    orig.set_label("changed");
    EXPECT_EQ(dest.label(), "orig");
}

// =========================================================================
// Equality
// =========================================================================

TEST(CppWrapper, EqualityOperator) {
    jgc::Triangle a, b;
    a.set_base(1.0f);
    a.set_height(2.0f);
    a.set_label("eq");

    b.set_base(1.0f);
    b.set_height(2.0f);
    b.set_label("eq");

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.set_height(3.0f);
    EXPECT_TRUE(a != b);
}

// =========================================================================
// Enum fields
// =========================================================================

TEST(CppWrapper, EnumAccessors) {
    jgc::EnumTestStruct e;
    e.set_color(Color_GREEN);
    e.set_status(Status_PENDING);
    e.set_value(99);

    EXPECT_EQ(e.color(), Color_GREEN);
    EXPECT_EQ(e.status(), Status_PENDING);
    EXPECT_EQ(e.value(), 99);
}

TEST(CppWrapper, EnumRoundTrip) {
    jgc::EnumTestStruct e;
    e.set_color(Color_BLUE);
    e.set_status(Status_ACTIVE);
    e.set_value(42);

    std::string json = e.marshal();
    jgc::EnumTestStruct e2 = jgc::EnumTestStruct::unmarshal(json);
    EXPECT_EQ(e2.color(), Color_BLUE);
    EXPECT_EQ(e2.status(), Status_ACTIVE);
    EXPECT_EQ(e2.value(), 42);
}

// =========================================================================
// Nested struct fields (raw C struct reference)
// =========================================================================

TEST(CppWrapper, NestedStructAccess) {
    jgc::NestedStruct ns;
    ns.set_id(1);
    ns.set_name("wrapper_test");

    // Access nested struct via C struct reference (returns ::TestStruct&)
    struct ::TestStruct& inner = ns.embedded();
    inner.int_val = 100;
    inner.sstr_val = sstr_new();
    sstr_append_cstr(inner.sstr_val, "inner_value");

    std::string json = ns.marshal();
    jgc::NestedStruct ns2 = jgc::NestedStruct::unmarshal(json);
    EXPECT_EQ(ns2.id(), 1);
    EXPECT_EQ(ns2.name(), "wrapper_test");
    EXPECT_EQ(ns2.embedded().int_val, 100);
    EXPECT_STREQ(sstr_cstr(ns2.embedded().sstr_val), "inner_value");
}

// =========================================================================
// Precise-width integers
// =========================================================================

TEST(CppWrapper, PreciseInts) {
    jgc::PreciseInts pi;
    pi.set_i8(-128);
    pi.set_i16(32000);
    pi.set_i32(-100000);
    pi.set_i64(1234567890123LL);
    pi.set_u8(255);
    pi.set_u16(65535);
    pi.set_u32(4000000000U);
    pi.set_u64(18000000000000000000ULL);

    std::string json = pi.marshal();
    jgc::PreciseInts pi2 = jgc::PreciseInts::unmarshal(json);
    EXPECT_EQ(pi2.i8(), -128);
    EXPECT_EQ(pi2.i16(), 32000);
    EXPECT_EQ(pi2.i32(), -100000);
    EXPECT_EQ(pi2.i64(), 1234567890123LL);
    EXPECT_EQ(pi2.u8(), 255);
    EXPECT_EQ(pi2.u16(), 65535);
    EXPECT_EQ(pi2.u32(), 4000000000U);
    EXPECT_EQ(pi2.u64(), 18000000000000000000ULL);
}

// =========================================================================
// Optional and nullable fields
// =========================================================================

TEST(CppWrapper, OptionalFields) {
    jgc::OptionalOnlyStruct opt;
    opt.set_id(1);
    // Optional fields start without has_ set
    // After marshalling from JSON with the field present, has_ should be set
    std::string json = R"({"id":1,"name":"test","score":99,"active":true,"rating":4.5,"precise":1.23,"big_num":99999})";
    opt.unmarshal_into(json);
    EXPECT_EQ(opt.id(), 1);
    EXPECT_TRUE(opt.has_name());
    EXPECT_TRUE(opt.has_score());
    EXPECT_TRUE(opt.has_active());
}

TEST(CppWrapper, NullableFields) {
    jgc::NullableOnlyStruct ns;
    std::string json = R"({"id":1,"name":null,"score":null,"active":null})";
    ns.unmarshal_into(json);
    EXPECT_EQ(ns.id(), 1);
    // Nullable fields: has_<field>() returns false when value is null
    EXPECT_FALSE(ns.has_name());
    EXPECT_FALSE(ns.has_score());
    EXPECT_FALSE(ns.has_active());
}

// =========================================================================
// Default values
// =========================================================================

TEST(CppWrapper, DefaultValues) {
    jgc::DefaultBasic db;
    // After init, defaults should be set by the generated init function
    std::string json = db.marshal();
    jgc::DefaultBasic db2 = jgc::DefaultBasic::unmarshal(json);
    EXPECT_EQ(db2.count(), 42);
    EXPECT_EQ(db2.big(), 1000000L);
    EXPECT_TRUE(db2.active());
    EXPECT_EQ(db2.label(), "hello");
}

// =========================================================================
// Alias fields
// =========================================================================

TEST(CppWrapper, AliasRoundTrip) {
    jgc::AliasBasic ab;
    ab.set_username("alice");
    ab.set_created(1700000000L);
    ab.set_id(42);

    std::string json = ab.marshal();
    // JSON should use aliases
    EXPECT_NE(json.find("user_name"), std::string::npos);
    EXPECT_NE(json.find("created_at"), std::string::npos);

    jgc::AliasBasic ab2 = jgc::AliasBasic::unmarshal(json);
    EXPECT_EQ(ab2.username(), "alice");
    EXPECT_EQ(ab2.created(), 1700000000L);
    EXPECT_EQ(ab2.id(), 42);
}

// =========================================================================
// C interop
// =========================================================================

TEST(CppWrapper, CStructInterop) {
    jgc::Triangle t;
    t.set_base(3.0f);
    t.set_height(4.0f);

    // Access via c_struct()
    struct Triangle& cs = t.c_struct();
    EXPECT_FLOAT_EQ(cs.base, 3.0f);
    EXPECT_FLOAT_EQ(cs.height, 4.0f);

    // Modify via C struct
    cs.base = 6.0f;
    EXPECT_FLOAT_EQ(t.base(), 6.0f);
}

// =========================================================================
// Oneof (tagged union) via C struct reference
// =========================================================================

TEST(CppWrapper, OneofFieldAccess) {
    jgc::Drawing d;
    d.set_name("my_drawing");

    // Set up oneof via C struct reference
    struct Shape& s = d.shape();
    s.tag = Shape_circle;
    s.value.circle.radius = 5.0f;

    std::string json = d.marshal();
    jgc::Drawing d2 = jgc::Drawing::unmarshal(json);
    EXPECT_EQ(d2.name(), "my_drawing");
    EXPECT_EQ(d2.shape().tag, Shape_circle);
    EXPECT_FLOAT_EQ(d2.shape().value.circle.radius, 5.0f);
}

// =========================================================================
// Dynamic array fields (raw pointer access)
// =========================================================================

TEST(CppWrapper, DynamicArrayViaJson) {
    std::string json = R"({"house":{"number":"42","street":"Main St"},"people":[{"name":"Alice","age":"30"},{"name":"Bob","age":"25"}]})";
    jgc::Data d = jgc::Data::unmarshal(json);

    EXPECT_EQ(d.people_len(), 2);
    EXPECT_STREQ(sstr_cstr(d.c_struct().people[0].name), "Alice");
    EXPECT_STREQ(sstr_cstr(d.c_struct().people[1].name), "Bob");
}

// =========================================================================
// Map fields (raw C struct access)
// =========================================================================

TEST(CppWrapper, MapViaJson) {
    std::string json = R"({"scores":{"alice":100,"bob":85}})";
    jgc::MapIntStruct m = jgc::MapIntStruct::unmarshal(json);

    auto& cm = m.scores();
    EXPECT_EQ(cm.len, 2);
}

// =========================================================================
// Complex struct with many field types
// =========================================================================

TEST(CppWrapper, ComplexStructRoundTrip) {
    std::string json = R"({
        "simple_int": 1, "simple_long": 2, "simple_float": 1.5,
        "simple_double": 2.5, "simple_bool": true,
        "simple_string": "complex",
        "int_array": [1, 2, 3],
        "long_array": [10, 20],
        "float_array": [1.1, 2.2],
        "double_array": [3.3],
        "string_array": ["a", "b"],
        "address": {"number": "1", "street": "Oak"},
        "contacts": [{"name": "Eve", "age": "28"}]
    })";

    jgc::ComplexStruct cs = jgc::ComplexStruct::unmarshal(json);
    EXPECT_EQ(cs.simple_int(), 1);
    EXPECT_EQ(cs.simple_long(), 2);
    EXPECT_FLOAT_EQ(cs.simple_float(), 1.5f);
    EXPECT_DOUBLE_EQ(cs.simple_double(), 2.5);
    EXPECT_EQ(cs.simple_bool(), true);
    EXPECT_EQ(cs.simple_string(), "complex");

    // Re-marshal and verify
    std::string json2 = cs.marshal();
    EXPECT_FALSE(json2.empty());

    jgc::ComplexStruct cs2 = jgc::ComplexStruct::unmarshal(json2);
    EXPECT_EQ(cs2.simple_int(), 1);
    EXPECT_EQ(cs2.simple_string(), "complex");
}
