#include <malloc.h>
#include <stdio.h>

#include "json.gen.h"
#include "sstr.h"

void example_scalar(void) {
    printf("example_scalar\n");
    struct D a;
    D_init(&a);
    a.double_val = 3.14;
    a.float_val = 2.718;
    a.int_val1 = 65536;
    a.long_val = 123456789;
    a.int_val2 = 1;
    a.bool_val = 1;
    a.sstr_val = sstr("hello\n\r\\ \" ''' world 你好世界");

    sstr_t json_out = sstr_new();
    json_marshal_indent_D(&a, 4, 0, json_out);
    printf("marshal a to json> %s\n", sstr_cstr(json_out));

    struct D b;
    json_unmarshal_D(json_out, &b);
    printf(
        "unmarshal b from json> double_val=%f float_val=%f int_val1=%d "
        "long_val=%ld int_val2=%d sstr_val=%s bool_val=%d\n",
        b.double_val, b.float_val, b.int_val1, b.long_val, b.int_val2,
        sstr_cstr(b.sstr_val), b.bool_val);

    sstr_t json_out2 = sstr_new();
    json_marshal_indent_D(&b, 4, 0, json_out2);
    printf("marshal b to json> %s\n", sstr_cstr(json_out2));

    sstr_free(json_out2);
    D_clear(&b);
    sstr_free(json_out);
    D_clear(&a);
}

void example_struct_array(void) {
    printf("example_scalar_array\n");
    struct D a[3];
    int i;
    for (i = 0; i < 3; i++) {
        D_init(&a[i]);
        a[i].double_val = 3.14;
        a[i].float_val = 2.718;
        a[i].int_val1 = 65536;
        a[i].long_val = 123456789;
        a[i].int_val2 = 1;
        a[i].sstr_val = sstr_printf("hello[%d]world", i);
    }
    sstr_t json_out = sstr_new();
    json_marshal_array_indent_D(a, 3, 4, 0, json_out);
    printf("marshal a[] to json> %s\n", sstr_cstr(json_out));

    struct D* b = NULL;
    int len = 0;
    json_unmarshal_array_D(json_out, &b, &len);
    printf("unmarshal b[] from json> len=%d\n", len);

    sstr_t json_out2 = sstr_new();
    json_marshal_array_indent_D(b, len, 4, 0, json_out2);
    printf("marshal b[] to json> %s\n", sstr_cstr(json_out2));
    sstr_free(json_out2);
    sstr_free(json_out);
    for (i = 0; i < len; i++) {
        D_clear(&b[i]);
    }
    free(b);
    for (i = 0; i < 3; i++) {
        D_clear(&a[i]);
    }
}

void example_struct_C() {
    printf("example_struct_C\n");
    struct C c;
    C_init(&c);
    int i;
    c.array_b_len = 3;
    c.array_b = (struct B*)malloc(sizeof(struct B) * c.array_b_len);
    for (i = 0; i < c.array_b_len; ++i) {
        B_init(&c.array_b[i]);
        struct B* b = &c.array_b[i];
        b->int_val_b = i;
        struct A* a = &b->inner_a;
        a->scalar.double_val = 3.14;
        a->scalar.float_val = 2.718;
        a->scalar.int_val1 = 65536;
        a->scalar.long_val = 123456789;
        a->scalar.int_val2 = 1;
        a->scalar.sstr_val = sstr_printf("hello[%d]world", i);
        a->intval_a = i;
        a->int_array_len = 3;
        a->int_array = (int*)malloc(sizeof(int) * a->int_array_len);
        int j;
        for (j = 0; j < a->int_array_len; ++j) {
            a->int_array[j] = j * 10 + i;
        }
        a->long_array_len = 0;
        a->long_array = NULL;
        a->double_array_len = 0;
        a->double_array = NULL;
        a->float_array_len = 0;
        a->float_array = NULL;
        a->sstr_array_len = 3;
        a->sstr_array = (sstr_t*)malloc(sizeof(sstr_t) * a->sstr_array_len);
        for (j = 0; j < a->sstr_array_len; ++j) {
            a->sstr_array[j] = sstr_printf("hello[%d]world", j * 111);
        }
    }

    sstr_t json_out = sstr_new();
    json_marshal_indent_C(&c, 4, 0, json_out);
    printf("marshal c to json> %s\n", sstr_cstr(json_out));

    struct C c2;
    C_init(&c2);
    json_unmarshal_C(json_out, &c2);

    sstr_t json_out2 = sstr_new();
    json_marshal_indent_C(&c2, 4, 0, json_out2);
    printf("marshal c2 to json> %s\n", sstr_cstr(json_out2));

    sstr_free(json_out2);
    sstr_free(json_out);
    C_clear(&c2);

    C_clear(&c);
}

int main() {
    example_scalar();
    printf("\n");
    example_struct_array();
    printf("\n");
    example_struct_C();
    printf("\n");
    return 0;
}
