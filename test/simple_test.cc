#include <gtest/gtest.h>
#include <stdio.h>  /* printf, scanf, puts, NULL */
#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */

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

TEST(unmarshal_int_array, random) {
    {
        srand(time(NULL));
        const int array_len = 100;
        for (int i = 0; i < 100; ++i) {
            int *array = (int *)malloc(array_len * sizeof(int));
            for (int j = 0; j < array_len; ++j) {
                array[j] = rand() * random();
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
