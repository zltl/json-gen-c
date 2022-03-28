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

TEST(unmarshal_int_array, simple) {
    {
        sstr_t json_body = sstr(R"(
[1, 2,3,  4,       5, 6, 7,    8,

9, 10




]
)");
        int *a = NULL;
        int len = 0;
        int r = json_unmarshal_array_int(json_body, &a, &len);
        ASSERT_EQ(r, 0) << "json_unmarshal_array_int failed";
        sstr_free(json_body);
        json_body = NULL;
        ASSERT_EQ(len, 10);
        for (int i = 0; i < len; ++i) {
            ASSERT_EQ(a[i], i + 1)
                << "unmarshal array a[" << i << "] != " << i + 1;
        }
        free(a);
        a = NULL;
    }

    {
        sstr_t json_body = sstr(R"(
[-1, -2,-3,  -4,      -5, -6, -7,    -8,

-9, -10
]
)");
        int *a = NULL;
        int len = 0;
        int r = json_unmarshal_array_int(json_body, &a, &len);
        ASSERT_EQ(r, 0) << "json_unmarshal_array_int failed";
        sstr_free(json_body);
        json_body = NULL;
        ASSERT_EQ(len, 10);
        for (int i = 0; i < len; ++i) {
            ASSERT_EQ(a[i], -(i + 1))
                << "unmarshal array a[" << i << "] != " << -(i + 1);
        }
        free(a);
        a = NULL;
    }
}

TEST(marshal_int_array, simple) {
    sstr_t json_body = sstr("[1,2,3,4,5,6,7,8,9,10]");
    int a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int len = 10;
    sstr_t out = sstr_new();
    int r = json_marshal_array_int(a, len, out);
    ASSERT_EQ(r, 0) << "json_marshal_array_int failed";
    ASSERT_TRUE(sstr_compare(json_body, out) == 0)
        << "marshal not match expetec\n"
        << sstr_cstr(json_body) << std::endl
        << sstr_cstr(out) << std::endl;
    sstr_free(json_body);
    sstr_free(out);
    json_body = NULL;
}

TEST(unmarshal_int_array, random) {
    {
        srand(time(NULL));
        const int array_len = 100;
        for (int i = 0; i < 100; ++i) {
            int *array = (int *)malloc(array_len * sizeof(int));
            for (int j = 0; j < array_len; ++j) {
                array[j] = rand() * rand();
            }
            sstr_t json_body = sstr_new();
            int r = json_marshal_array_int(array, array_len, json_body);
            ASSERT_EQ(r, 0) << "json_marshal_array_int failed";
            int *a = NULL;
            int len = 0;
            r = json_unmarshal_array_int(json_body, &a, &len);
            ASSERT_EQ(r, 0) << "json_unmarshal_array_int failed";
            sstr_free(json_body);
            json_body = NULL;
            ASSERT_EQ(len, array_len);
            for (int j = 0; j < array_len; ++j) {
                ASSERT_EQ(a[j], array[j])
                    << "unmarshal array a[" << j << "] != " << array[j];
            }
            free(a);
            a = NULL;
            free(array);
            array = NULL;
        }
    }
}

TEST(unmarshal_long_array, simple) {
    {
        sstr_t json_body = sstr(R"(
[1, 2,3,  4,       5, 6, 7,    8,

9, 10




]
)");
        long *a = NULL;
        int len = 0;
        int r = json_unmarshal_array_long(json_body, &a, &len);
        ASSERT_EQ(r, 0) << "json_unmarshal_array_long failed";
        sstr_free(json_body);
        json_body = NULL;
        ASSERT_EQ(len, 10);
        for (int i = 0; i < len; ++i) {
            ASSERT_EQ(a[i], i + 1)
                << "unmarshal array a[" << i << "] != " << i + 1;
        }
        free(a);
        a = NULL;
    }

    {
        sstr_t json_body = sstr(R"(
[-1, -2,-3,  -4,      -5, -6, -7,    -8,

-9, -10
]
)");
        long *a = NULL;
        int len = 0;
        int r = json_unmarshal_array_long(json_body, &a, &len);
        ASSERT_EQ(r, 0) << "json_unmarshal_array_long failed";
        sstr_free(json_body);
        json_body = NULL;
        ASSERT_EQ(len, 10);
        for (int i = 0; i < len; ++i) {
            ASSERT_EQ(a[i], -(i + 1))
                << "unmarshal array a[" << i << "] != " << -(i + 1);
        }
        free(a);
        a = NULL;
    }
}

TEST(unmarshal_long_array, random) {
    {
        srand(time(NULL));
        const int array_len = 100;
        for (int i = 0; i < 100; ++i) {
            long *array = (long *)malloc(array_len * sizeof(long));
            for (int j = 0; j < array_len; ++j) {
                array[j] = rand() * rand();
            }
            sstr_t json_body = sstr_new();
            int r = json_marshal_array_long(array, array_len, json_body);
            ASSERT_EQ(r, 0) << "json_marshal_array_long failed";
            long *a = NULL;
            int len = 0;
            r = json_unmarshal_array_long(json_body, &a, &len);
            ASSERT_EQ(r, 0) << "json_unmarshal_array_long failed";
            sstr_free(json_body);
            json_body = NULL;
            ASSERT_EQ(len, array_len);
            for (int j = 0; j < array_len; ++j) {
                ASSERT_EQ(a[j], array[j])
                    << "unmarshal array a[" << j << "] != " << array[j];
            }
            free(a);
            a = NULL;
            free(array);
            array = NULL;
        }
    }
}

TEST(marshal_long_array, simple) {
    sstr_t json_body = sstr("[1,2,3,4,5,6,7,8,9,10]");
    long a[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int len = 10;
    sstr_t out = sstr_new();
    int r = json_marshal_array_long(a, len, out);
    ASSERT_EQ(r, 0) << "json_marshal_array_long failed";
    ASSERT_TRUE(sstr_compare(json_body, out) == 0)
        << "marshal not match expetec\n"
        << sstr_cstr(json_body) << std::endl
        << sstr_cstr(out) << std::endl;
    sstr_free(json_body);
    sstr_free(out);
    json_body = NULL;
}

TEST(unmarshal_float_array, random) {
    {
        srand(time(NULL));
        const int array_len = 100;
        for (int i = 0; i < 100; ++i) {
            float *array = (float *)malloc(array_len * sizeof(float));
            for (int j = 0; j < array_len; ++j) {
                array[j] = ((float)rand()) / (float)RAND_MAX * 10000;
            }
            sstr_t json_body = sstr_new();
            int r = json_marshal_array_float(array, array_len, json_body);
            // printf(">> %s\n", sstr_cstr(json_body));
            ASSERT_EQ(r, 0) << "json_marshal_array_float failed";
            float *a = NULL;
            int len = 0;
            r = json_unmarshal_array_float(json_body, &a, &len);
            ASSERT_EQ(r, 0) << "json_unmarshal_array_float failed";
            sstr_free(json_body);
            json_body = NULL;
            ASSERT_EQ(len, array_len);
            for (int j = 0; j < array_len; ++j) {
                ASSERT_TRUE(abs(a[j] - array[j]) < 1e-6)
                    << "unmarshal array a[" << j << "]:" << a[j]
                    << " != " << array[j];
            }
            free(a);
            a = NULL;
            free(array);
            array = NULL;
        }
    }
}

TEST(marshal_float_array, simple) {
    float a[] = {3.141590,    2.718281,    3123.123047, 45.340000, 0.222500,
                 9006.000000, 1231.699951, 0.800000,    92.250000, 83.916664};
    int len = 10;
    sstr_t out = sstr_new();
    int r = json_marshal_array_float(a, len, out);
    ASSERT_EQ(r, 0) << "json_marshal_array_float failed";
    printf("%s\n", sstr_cstr(out));

    float *b = NULL;
    int blen = 0;
    r = json_unmarshal_array_float(out, &b, &blen);
    ASSERT_EQ(r, 0) << "json_unmarshal_array_float failed";
    ASSERT_EQ(blen, len);
    for (int i = 0; i < len; ++i) {
        ASSERT_TRUE(abs(a[i] - b[i]) < 1e-6)
            << "unmarshal array a[" << i << "]:" << a[i] << " != " << b[i];
    }
    sstr_free(out);
    free(b);
}

TEST(marshal_double_array, simple) {
    double a[] = {3.141590,    2.718281,    3123.123047, 45.340000, 0.222500,
                  9006.000000, 1231.699951, 0.800000,    92.250000, 83.916664};
    int len = 10;
    sstr_t out = sstr_new();
    int r = json_marshal_array_double(a, len, out);
    ASSERT_EQ(r, 0) << "json_marshal_array_double failed";
    printf("%s\n", sstr_cstr(out));

    double *b = NULL;
    int blen = 0;
    r = json_unmarshal_array_double(out, &b, &blen);
    ASSERT_EQ(r, 0) << "json_unmarshal_array_double failed";
    ASSERT_EQ(blen, len);
    for (int i = 0; i < len; ++i) {
        ASSERT_TRUE(abs(a[i] - b[i]) < 1e-6)
            << "unmarshal array a[" << i << "]:" << a[i] << " != " << b[i];
    }
    sstr_free(out);
    free(b);
}

TEST(unmarshal_double_array, random) {
    {
        srand(time(NULL));
        const int array_len = 100;
        for (int i = 0; i < 100; ++i) {
            double *array = (double *)malloc(array_len * sizeof(double));
            for (int j = 0; j < array_len; ++j) {
                array[j] = ((double)rand()) / (double)RAND_MAX * 10000;
            }
            sstr_t json_body = sstr_new();
            int r = json_marshal_array_double(array, array_len, json_body);
            // printf(">> %s\n", sstr_cstr(json_body));
            ASSERT_EQ(r, 0) << "json_marshal_array_double failed";
            double *a = NULL;
            int len = 0;
            r = json_unmarshal_array_double(json_body, &a, &len);
            ASSERT_EQ(r, 0) << "json_unmarshal_array_double failed";
            sstr_free(json_body);
            json_body = NULL;
            ASSERT_EQ(len, array_len);
            for (int j = 0; j < array_len; ++j) {
                ASSERT_TRUE(abs(a[j] - array[j]) < 1e-6)
                    << "unmarshal array a[" << j << "]:" << a[j]
                    << " != " << array[j];
            }
            free(a);
            a = NULL;
            free(array);
            array = NULL;
        }
    }
}

TEST(_un_marshal_sstr_t_array, simple) {
    {
        sstr_t json_body = sstr(R"(
["first element", "second element", "third element", "
dk
cd
你好", "fifth element"]
)");
        std::vector<std::string> exp{"first element", "second element",
                                     "third element", "\ndk\ncd\n你好",
                                     "fifth element"};
        sstr_t *a = NULL;
        int len = 0;
        int r = json_unmarshal_array_sstr_t(json_body, &a, &len);
        ASSERT_EQ(r, 0) << "json_unmarshal_array_sstr_t failed";
        sstr_free(json_body);
        json_body = NULL;
        ASSERT_EQ(len, 5);

        for (int i = 0; i < len; ++i) {
            ASSERT_TRUE(exp[i] == sstr_cstr(a[i]))
                << "unmarshal array[" << sstr_cstr(a[i]) << "]!=[" << exp[i]
                << ']';
        }

        sstr_t exp_out = sstr(
            R"(["first element","second element","third element","\ndk\ncd\n你好","fifth element"])");
        sstr_t out = sstr_new();
        json_marshal_array_sstr_t(a, len, out);
        ASSERT_TRUE(sstr_compare(exp_out, out) == 0);
        sstr_free(exp_out);
        sstr_free(out);
        for (int i = 0; i < len; ++i) {
            sstr_free(a[i]);
        }
        free(a);
        a = NULL;
    }
}
