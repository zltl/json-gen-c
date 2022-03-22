#include <malloc.h>
#include <stdio.h>

#include "json.gen.h"
#include "sstr.h"

int main() {
    struct ABCde_123 a;
    ABCde_123_init(&a);
    a.xxxa = 123;
    a.b = sstr("hello");
    a.c = (long*)malloc(sizeof(long) * 3);
    a.c[0] = 1;
    a.c[1] = 2;
    a.c[2] = 3;
    a.c_len = 3;
    a.d = 1.245;
    a.xyz = (struct XYzC*)malloc(sizeof(struct XYzC) * 2);
    a.xyz_len = 2;
    a.xyz[0].cdz = 1.23;
    a.xyz[1].cdz = 3.14159;

    sstr_t out = sstr_new();
    marshal_ABCde_123(&a, out);
    printf("marshal a> %s\n", sstr_cstr(out));

    struct ABCde_123 b;
    ABCde_123_init(&b);
    unmarshal_ABCde_123(out, &b);

    sstr_t out2 = sstr_new();
    marshal_ABCde_123(&b, out2);
    printf("marshal b> %s\n", sstr_cstr(out2));

    sstr_free(out2);
    sstr_free(out);
    ABCde_123_clear(&a);
    ABCde_123_clear(&b);

    return 0;
}
