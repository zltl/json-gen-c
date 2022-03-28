#include <gtest/gtest.h>
#include <stdio.h>  /* printf, scanf, puts, NULL */
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "json.gen.h"
#include "sstr.h"

TEST(struct_marshal, simple) {
    struct TestStruct a;
    TestStruct_init(&a);
    a.bool_val = 1;
    a.double_val = 3.14159;
    a.float_val = 2.718281;
    a.int_val = -1;
    a.long_val = 123;
    a.sstr_val = sstr("this is a string value");

    sstr_t out_json = sstr_new();

    int r = json_marshal_TestStruct(&a, out_json);
    ASSERT_EQ(r, 0);
    printf("%s\n", sstr_cstr(out_json));

    struct TestStruct b;
    TestStruct_init(&b);
    r = json_unmarshal_TestStruct(out_json, &b);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(a.bool_val, b.bool_val);
    ASSERT_TRUE(abs(a.double_val - b.double_val) < 0.00001);
    ASSERT_TRUE(abs(a.float_val - b.float_val) < 1e-6);
    ASSERT_EQ(a.int_val, b.int_val);
    ASSERT_EQ(a.long_val, b.long_val);
    ASSERT_TRUE(sstr_compare(a.sstr_val, b.sstr_val) == 0);

    sstr_free(out_json);
    TestStruct_clear(&a);
    TestStruct_clear(&b);
}

TEST(struct_marshal, array) {
    struct TestStruct a[3];
    for (int i = 0; i < 3; ++i) {
        TestStruct_init(&a[i]);
        a[i].bool_val = i % 2;
        a[i].double_val = 3.14159;
        a[i].float_val = 2.718281;
        a[i].int_val = -1;
        a[i].long_val = 123;
        a[i].sstr_val = sstr("this is a string value");
    }

    sstr_t out_json = sstr_new();

    int len = 3;
    int r = json_marshal_array_TestStruct(a, len, out_json);
    ASSERT_EQ(r, 0);
    printf("%s\n", sstr_cstr(out_json));

    struct TestStruct *b = NULL;
    int b_len = 0;
    r = json_unmarshal_array_TestStruct(out_json, &b, &b_len);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(len, b_len);
    for (int i = 0; i < len; ++i) {
        ASSERT_EQ(a[i].bool_val, b[i].bool_val);
        ASSERT_TRUE(abs(a[i].double_val - b[i].double_val) < 0.00001);
        ASSERT_TRUE(abs(a[i].float_val - b[i].float_val) < 1e-6);
        ASSERT_EQ(a[i].int_val, b[i].int_val);
        ASSERT_EQ(a[i].long_val, b[i].long_val);
        ASSERT_TRUE(sstr_compare(a[i].sstr_val, b[i].sstr_val) == 0);
        TestStruct_clear(&a[i]);
        TestStruct_clear(&b[i]);
    }
    free(b);
    sstr_free(out_json);
}
