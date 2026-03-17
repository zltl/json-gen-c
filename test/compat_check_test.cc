/**
 * @file compat_check_test.cc
 * @brief Tests for the schema compatibility checker (--check-compat).
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "compat/compat_check.h"
#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"
}

class CompatCheckTest : public ::testing::Test {
protected:
    struct struct_parser *old_p = nullptr;
    struct struct_parser *new_p = nullptr;

    void SetUp() override {
        old_p = struct_parser_new();
        new_p = struct_parser_new();
        ASSERT_NE(nullptr, old_p);
        ASSERT_NE(nullptr, new_p);
        old_p->name = const_cast<char *>("old.json-gen-c");
        new_p->name = const_cast<char *>("new.json-gen-c");
    }

    void TearDown() override {
        if (old_p) struct_parser_free(old_p);
        if (new_p) struct_parser_free(new_p);
    }

    int parse_old(const char *schema) {
        sstr_t s = sstr(schema);
        int r = struct_parser_parse(old_p, s);
        sstr_free(s);
        return r;
    }

    int parse_new(const char *schema) {
        sstr_t s = sstr(schema);
        int r = struct_parser_parse(new_p, s);
        sstr_free(s);
        return r;
    }

    int run_check() {
        return compat_check(old_p->struct_map, old_p->enum_map,
                            old_p->oneof_map, new_p->struct_map,
                            new_p->enum_map, new_p->oneof_map);
    }
};

TEST_F(CompatCheckTest, IdenticalSchemas) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; sstr_t name; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; sstr_t name; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, AddFieldSafe) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; int y; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, AddOptionalFieldSafe) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; optional int y; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, DeprecateFieldSafe) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; int old; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; @deprecated int old; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, AddEnumValueSafe) {
    ASSERT_EQ(0, parse_old("enum Color { RED, GREEN }"));
    ASSERT_EQ(0, parse_new("enum Color { RED, GREEN, BLUE }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, DeprecateEnumValueSafe) {
    ASSERT_EQ(0, parse_old("enum Color { RED, GREEN }"));
    ASSERT_EQ(0, parse_new("enum Color { RED, @deprecated GREEN }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, AddStructSafe) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; }\nstruct Bar { int y; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, RemoveFieldBreaking) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; sstr_t name; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, ChangeFieldTypeBreaking) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { long x; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, ChangeFieldArraynessBreaking) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x[]; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, RemoveEnumValueBreaking) {
    ASSERT_EQ(0, parse_old("enum Color { RED, GREEN, BLUE }"));
    ASSERT_EQ(0, parse_new("enum Color { RED, BLUE }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, RemoveStructBreaking) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }\nstruct Bar { int y; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, RemoveEnumBreaking) {
    ASSERT_EQ(0, parse_old("enum Color { RED, GREEN }"));
    ASSERT_EQ(0, parse_new("struct Unrelated { int x; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, MixedSafeAndBreaking) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; sstr_t old; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; optional int new_field; }"));
    // 'old' removed (breaking) + 'new_field' added (safe) => breaking
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, OneofAddVariantSafe) {
    ASSERT_EQ(0, parse_old(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; }"));
    ASSERT_EQ(0, parse_new(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; B vb; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, OneofRemoveVariantBreaking) {
    ASSERT_EQ(0, parse_old(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; B vb; }"));
    ASSERT_EQ(0, parse_new(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; }"));
    EXPECT_EQ(1, run_check());
}

TEST_F(CompatCheckTest, OneofDeprecateVariantSafe) {
    ASSERT_EQ(0, parse_old(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; B vb; }"));
    ASSERT_EQ(0, parse_new(
        "struct A { int a; }\n"
        "struct B { int b; }\n"
        "oneof Shape { A va; @deprecated B vb; }"));
    EXPECT_EQ(0, run_check());
}

TEST_F(CompatCheckTest, AddEnumSafe) {
    ASSERT_EQ(0, parse_old("struct Foo { int x; }"));
    ASSERT_EQ(0, parse_new("struct Foo { int x; }\nenum Color { RED }"));
    EXPECT_EQ(0, run_check());
}
