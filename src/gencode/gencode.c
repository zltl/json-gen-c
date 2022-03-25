#include "gencode/gencode.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "extra_codes.inc"
#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"

#define DEPENDENCY_HASH_MAP_BUCKET_SIZE 1280

static unsigned int hash_s(const char* data, size_t n, unsigned int seed) {
    // unsigned int seed = 0xbc9f1d34;
    // Similar to murmur hash
    const unsigned int m = 0xc6a4a793;
    const unsigned int r = 24;
    const char* limit = data + n;
    unsigned int h = seed ^ (n * m);

    // Pick up four bytes at a time
    while (data + 4 <= limit) {
        unsigned int w = *(unsigned int*)(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    // Pick up remaining bytes
    switch (limit - data) {
        case 3:
            h += (unsigned char)(data[2]) << 16;
            __attribute__((fallthrough));
        case 2:
            h += (unsigned char)(data[1]) << 8;
            __attribute__((fallthrough));
        case 1:
            h += (unsigned char)(data[0]);
            h *= m;
            h ^= (h >> r);
            break;
    }
    return h;
}

inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int res = 0xbc9f1d34;
    sstr_t tmp = sstr_dup(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append(tmp, key2);
    res = hash_s(sstr_cstr(tmp), sstr_length(tmp), res);
    sstr_free(tmp);
    return res;
}

static void gen_code_struct_marshal_array(struct struct_container* st,
                                          sstr_t source);

static void gen_code_struct_header(struct struct_container* st, sstr_t header) {
    (void)header;
    sstr_append_cstr(header, "struct ");
    sstr_append(header, st->name);
    sstr_append_cstr(header, " {\n");
    struct struct_field* field = st->fields;
    while (field) {
        sstr_append_cstr(header, "    ");
        if (field->type == FIELD_TYPE_STRUCT) {
            sstr_append_cstr(header, "struct ");
        }
        sstr_append(header, field->type_name);
        if (field->is_array) {
            sstr_append_cstr(header, "*");
        }
        sstr_append_cstr(header, " ");
        sstr_append(header, field->name);
        sstr_append_cstr(header, ";\n");

        if (field->is_array) {
            sstr_printf_append(header, "    int %S_len;\n", field->name);
        }

        field = field->next;
    }
    sstr_append_cstr(header, "};\n\n");

    sstr_printf_append(header, "int %S_init(struct %S*obj);\n", st->name,
                       st->name);
    sstr_printf_append(header, "int %S_clear(struct %S*obj);\n", st->name,
                       st->name);
    sstr_printf_append(header,
                       "int json_marshal_%S(struct %S* obj, sstr_t out);\n",
                       st->name, st->name);
    sstr_printf_append(
        header,
        "int json_marshal_array_%S(struct %S* obj, int len, sstr_t out);\n",
        st->name, st->name);

    sstr_printf_append(header,
                       "int json_unmarshal_%S(sstr_t in, struct %S* obj);\n",
                       st->name, st->name);
    sstr_printf_append(header,
                       "int json_unmarshal_array_%S(sstr_t in, struct %S** "
                       "obj, int* len);\n",
                       st->name, st->name);
}

static void gen_code_struct_unmarshal_struct(struct struct_container* st,
                                             sstr_t source) {
    sstr_printf_append(source,
                       "int json_unmarshal_%S(sstr_t in, struct %S* obj) {\n",
                       st->name, st->name);
    sstr_append_cstr(source,
                     "    struct json_pos pos;\n"
                     "    pos.col = 0; pos.line = 0; pos.offset = 0;\n"
                     "    struct json_parse_param param;\n"
                     "    param.instance_ptr = obj;\n"
                     "    param.field_name = \"\";\n"
                     "    param.in_array = 0;\n"
                     "    param.in_struct = 1;\n");
    sstr_printf_append(source, "    param.struct_name = \"%S\";\n", st->name);
    sstr_append_cstr(
        source,
        "    sstr_t txt = sstr_new();\n"
        "    int r = json_unmarshal_struct_internal(in, &pos, &param, txt);\n"
        "    if (r < 0) {\n"
        "#ifdef JSON_DEBUG\n"
        "        printf(\"ERROR: %s\", sstr_cstr(txt));\n"
        "#endif\n"
        "    }\n"
        "    sstr_free(txt);\n"
        "    return r;\n"
        "}\n\n");
}

static void gen_code_struct_unmarshal_array_struct(struct struct_container* st,
                                                   sstr_t source) {
    sstr_printf_append(source,
                       "int json_unmarshal_array_%S(sstr_t in, struct %S** "
                       "obj, int *len) {\n",
                       st->name, st->name);
    sstr_append_cstr(source,
                     "    *len = 0;\n"
                     "    sstr_t txt = sstr_new();\n"
                     "    struct json_pos pos;\n"
                     "    pos.col = 0; pos.line = 0; pos.offset = 0;\n"
                     "    struct json_parse_param ar_param;\n"
                     "    ar_param.instance_ptr = obj;\n"
                     "    ar_param.in_array = 1;\n"
                     "    ar_param.in_struct = 0;\n");
    sstr_printf_append(source, "    ar_param.struct_name = \"%S\";\n",
                       st->name);
    sstr_append_cstr(source,
                     "    ar_param.field_name=\"\";\n"
                     "    int r = json_unmarshal_array_internal(in, &pos, "
                     "&ar_param, len, txt);\n");
    sstr_append_cstr(source, "    sstr_free(txt);\n");
    sstr_append_cstr(source, "    return r;\n");
    sstr_append_cstr(source, "}\n\n");
}

static void gen_code_struct_marshal_struct(struct struct_container* st,
                                           sstr_t source) {
    sstr_printf_append(source,
                       "int json_marshal_%S(struct %S* obj, sstr_t out) {\n",
                       st->name, st->name);
    sstr_append_cstr(source, "    char tmp_cstr[64];\n    (void)tmp_cstr;\n");

    sstr_append_cstr(source, "    sstr_append_cstr(out, \"{\");\n");
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        sstr_printf_append(source,
                           "    sstr_append_cstr(out, \"\\\"%S\\\":\");\n",
                           field->name);

        if (field->is_array) {
            sstr_printf_append(
                source,
                "    json_marshal_array_%S(obj->%S, obj->%S_len, out);\n",
                field->type_name, field->name, field->name);
            if (field->next != NULL) {
                sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
            }
            continue;
        }
        switch (field->type) {
            case FIELD_TYPE_BOOL:
                sstr_printf_append(source,
                                   "    sprintf(tmp_cstr, \"%%d\", obj->%S);\n",
                                   field->name);
                sstr_printf_append(source, "    if (obj->%S) {\n", field->name);
                sstr_append_cstr(source,
                                 "        sstr_append_cstr(out, \"true\");\n");
                sstr_append_cstr(source, "    } else {\n");
                sstr_append_cstr(source,
                                 "        sstr_append_cstr(out, \"false\");\n");
                sstr_append_cstr(source, "    }\n");
                break;
            case FIELD_TYPE_INT:
                sstr_printf_append(source,
                                   "    sprintf(tmp_cstr, \"%%d\", obj->%S);\n",
                                   field->name);
                sstr_append_cstr(source,
                                 "    sstr_append_cstr(out, tmp_cstr);\n");
                break;
            case FIELD_TYPE_LONG:
                sstr_printf_append(
                    source, "    sprintf(tmp_cstr, \"%%ld\", obj->%S);\n",
                    field->name);
                sstr_append_cstr(source,
                                 "    sstr_append_cstr(out, tmp_cstr);\n");
                break;
            case FIELD_TYPE_FLOAT:
                sstr_printf_append(source,
                                   "    sprintf(tmp_cstr, \"%%f\", obj->%S);\n",
                                   field->name);
                sstr_append_cstr(source,
                                 "    sstr_append_cstr(out, tmp_cstr);\n");

                break;
            case FIELD_TYPE_DOUBLE:
                sstr_printf_append(
                    source, "    sprintf(tmp_cstr, \"%%lf\", obj->%S);\n",
                    field->name);
                sstr_append_cstr(source,
                                 "    sstr_append_cstr(out, tmp_cstr);\n");
                break;
            case FIELD_TYPE_SSTR:
                sstr_printf_append(
                    source,
                    "    sstr_append_of(out, \"\\\"\", 1);\n"
                    "    sstr_json_escape_string_append(out, obj->%S);\n"
                    "    sstr_append_of(out, \"\\\"\", 1);\n",
                    field->name);
                break;
            case FIELD_TYPE_STRUCT:
                sstr_printf_append(source,
                                   "    json_marshal_%S(&obj->%S, out);\n",
                                   field->type_name, field->name);
                break;
        }

        if (field->next == NULL) {
        } else {
            sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
        }
    }
    sstr_append_cstr(source, "    sstr_append_of(out, \"}\", 1);\n");
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

static void gen_code_struct_marshal_array(struct struct_container* st,
                                          sstr_t source) {
    sstr_printf_append(
        source,
        "int json_marshal_array_%S(struct %S* obj, int len, sstr_t out) {\n",
        st->name, st->name);
    //
    sstr_append_cstr(source, "    int i;\n");
    sstr_append_cstr(source, "    sstr_append_of(out, \"[\", 1);\n");
    sstr_append_cstr(source, "    for (i = 0; i < len; i++) {\n");
    sstr_append_cstr(source, "        if (i != 0) {\n");
    sstr_append_cstr(source, "            sstr_append_of(out, \",\", 1);\n");
    sstr_append_cstr(source, "        }\n");
    sstr_printf_append(source, "        json_marshal_%S(&obj[i], out);\n",
                       st->name);
    sstr_append_cstr(source, "    }\n");
    sstr_append_cstr(source, "    sstr_append_of(out, \"]\", 1);\n");
    sstr_append_cstr(source, "\n    return 0;\n}\n\n");
}

static void gen_code_scalar_marshal_array(sstr_t source) {
    // int
    sstr_append_cstr(
        source,
        "int json_marshal_array_int(int*obj, int len, sstr_t out) {\n"
        "    int i;\n"
        "    sstr_append_of(out, \"[\", 1);\n"
        "    for (i = 0; i < len; i++) {\n"
        "        if (i != 0) {\n"
        "            sstr_append_of(out, \",\", 1);\n"
        "        }\n"
        "        sstr_printf_append(out, \"%d\", obj[i]);\n"
        "    }\n"
        "    sstr_append_of(out, \"]\", 1);\n"
        "    return 0;\n"
        "}\n\n");
    sstr_append_cstr(
        source,
        "int json_marshal_array_long(long*obj, int len, sstr_t out) {\n"
        "    int i;\n"
        "    sstr_append_of(out, \"[\", 1);\n"
        "    for (i = 0; i < len; i++) {\n"
        "        if (i != 0) {\n"
        "            sstr_append_of(out, \",\", 1);\n"
        "        }\n"
        "        sstr_printf_append(out, \"%l\", obj[i]);\n"
        "    }\n"
        "    sstr_append_of(out, \"]\", 1);\n"
        "    return 0;\n"
        "}\n\n");
    sstr_append_cstr(
        source,
        "int json_marshal_array_float(float*obj, int len, sstr_t out) {\n"
        "    int i;\n"
        "    sstr_append_of(out, \"[\", 1);\n"
        "    for (i = 0; i < len; i++) {\n"
        "        if (i != 0) {\n"
        "            sstr_append_of(out, \",\", 1);\n"
        "        }\n"
        "        sstr_printf_append(out, \"%f\", (double)obj[i]);\n"
        "    }\n"
        "    sstr_append_of(out, \"]\", 1);\n"
        "    return 0;\n"
        "}\n\n");
    sstr_append_cstr(
        source,
        "int json_marshal_array_double(double*obj, int len, sstr_t out) {\n"
        "    int i;\n"
        "    sstr_append_of(out, \"[\", 1);\n"
        "    for (i = 0; i < len; i++) {\n"
        "        if (i != 0) {\n"
        "            sstr_append_of(out, \",\", 1);\n"
        "        }\n"
        "        sstr_printf_append(out, \"%f\", obj[i]);\n"
        "    }\n"
        "    sstr_append_of(out, \"]\", 1);\n"
        "    return 0;\n"
        "}\n\n");
    sstr_append_cstr(
        source,
        "int json_marshal_array_sstr_t(sstr_t*obj, int len, sstr_t out) {\n"
        "    int i;\n"
        "    sstr_append_of(out, \"[\", 1);\n"
        "    for (i = 0; i < len; i++) {\n"
        "        if (i != 0) {\n"
        "            sstr_append_of(out, \",\", 1);\n"
        "        }\n"
        "        sstr_printf_append(out, \"\\\"%S\\\"\", obj[i]);\n"
        "    }\n"
        "    sstr_append_of(out, \"]\", 1);\n"
        "    return 0;\n"
        "}\n\n");
}

static void gen_code_struct_init(struct struct_container* st, sstr_t source) {
    sstr_printf_append(source, "int %S_init(struct %S*obj) {\n", st->name,
                       st->name);
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        if (field->is_array) {
            sstr_printf_append(source, "    obj->%S = NULL;\n", field->name);
            sstr_printf_append(source, "    obj->%S_len = 0;\n", field->name);
            continue;
        }
        switch (field->type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                break;
            case FIELD_TYPE_LONG:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                break;
            case FIELD_TYPE_FLOAT:
                sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                break;
            case FIELD_TYPE_DOUBLE:
                sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                break;
            case FIELD_TYPE_SSTR:
                sstr_printf_append(source, "    obj->%S = NULL;\n",
                                   field->name);
                break;
            case FIELD_TYPE_STRUCT:
                sstr_printf_append(source, "    %S_init(&obj->%S);\n",
                                   field->type_name, field->name);
                break;
        }
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

static void gen_code_struct_clear(struct struct_container* st, sstr_t source) {
    sstr_printf_append(source, "int %S_clear(struct %S*obj) {\n", st->name,
                       st->name);
    int have_i = 0;
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        if (field->is_array) {
            if (field->type == FIELD_TYPE_STRUCT ||
                field->type == FIELD_TYPE_SSTR) {
                if (!have_i) {
                    have_i = 1;
                    sstr_append_cstr(source, "    int i;\n");
                }
                sstr_printf_append(source,
                                   "    for (i = 0; i < obj->%S_len; i++) {\n",
                                   field->name);
                if (field->type == FIELD_TYPE_STRUCT) {
                    sstr_printf_append(source,
                                       "       %S_clear(&obj->%S[i]);\n",
                                       field->type_name, field->name);
                } else {
                    sstr_printf_append(
                        source, "       sstr_free(obj->%S[i]);\n", field->name);
                }
                sstr_append_cstr(source, "    }\n");
            }
            sstr_printf_append(source, "    free(obj->%S);\n", field->name);
            sstr_printf_append(source, "    obj->%S = NULL;\n", field->name);
            sstr_printf_append(source, "    obj->%S_len = 0;\n", field->name);
            continue;
        }
        switch (field->type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                break;
            case FIELD_TYPE_LONG:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                break;
            case FIELD_TYPE_FLOAT:
                sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                break;
            case FIELD_TYPE_DOUBLE:
                sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                break;
            case FIELD_TYPE_SSTR:
                sstr_printf_append(source, "    sstr_free(obj->%S);\n",
                                   field->name);
                sstr_printf_append(source, "    obj->%S = NULL;\n",
                                   field->name);
                break;
            case FIELD_TYPE_STRUCT:
                sstr_printf_append(source, "    %S_clear(&obj->%S);\n",
                                   field->type_name, field->name);
                break;
        }
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

static void gen_code_struct(struct struct_container* st, sstr_t source,
                            sstr_t header) {
    gen_code_struct_header(st, header);
    gen_code_struct_init(st, source);
    gen_code_struct_clear(st, source);
    gen_code_struct_marshal_struct(st, source);
    gen_code_struct_unmarshal_struct(st, source);
    gen_code_struct_unmarshal_array_struct(st, source);
    gen_code_struct_marshal_array(st, source);
}

static void count_fields_fd(void* key, void* value, void* ptr) {
    (void)key;
    struct struct_container* st = (struct struct_container*)value;
    int* count = (int*)ptr;
    struct struct_field* field = st->fields;
    while (field) {
        if (field->is_array) {
            (*count)++;
        }
        (*count)++;
        field = field->next;
    }
}

struct gen_fields_list_fn_param {
    sstr_t source;
    sstr_t header;
    int hash_size;
    int* hash_arr;
    int f_cnt;
};

static void gen_hash_arr(sstr_t struct_name, sstr_t field_name,
                         struct gen_fields_list_fn_param* param) {
    // gen field hash
    unsigned int hash_r = hash_2s(struct_name, field_name);
    int hash_i = hash_r % param->hash_size;
    while (param->hash_arr[hash_i] != -1) {
        hash_i++;
        if (hash_i >= param->hash_size) {
            hash_i = 0;
        }
    }
    param->hash_arr[hash_i] = param->f_cnt++;
}

static void gen_fields_list_fn(void* key, void* value, void* ptr) {
    (void)key;
    struct struct_container* st = (struct struct_container*)value;
    struct gen_fields_list_fn_param* param =
        (struct gen_fields_list_fn_param*)ptr;
    struct struct_field* field = st->fields;

    sstr_printf_append(
        param->source,
        "    {0, sizeof(struct %S), %d, \"\", \"\", \"%S\", 0},\n", st->name,
        FIELD_TYPE_STRUCT, st->name, st->name);
    sstr_t empty_s = sstr_new();
    gen_hash_arr(st->name, empty_s, param);
    sstr_free(empty_s);

    while (field) {
        // field list
        if (field->type == FIELD_TYPE_STRUCT) {
            sstr_printf_append(param->source,
                               "    {offsetof(struct %S, %S), sizeof(struct "
                               "%S), %d, \"%S\", \"%S\", "
                               "\"%S\", %d},\n",
                               st->name, field->name, field->type_name,
                               field->type, field->type_name, field->name,
                               st->name, field->is_array);
            gen_hash_arr(st->name, field->name, param);
        } else {
            sstr_printf_append(
                param->source,
                "    {offsetof(struct %S, %S), sizeof(%S), %d, \"%S\", \"%S\", "
                "\"%S\", %d},\n",
                st->name, field->name, field->type_name, field->type,
                field->type_name, field->name, st->name, field->is_array);
            gen_hash_arr(st->name, field->name, param);
        }
        if (field->is_array) {
            sstr_printf_append(param->source,
                               "    {offsetof(struct %S, %S_len), sizeof(int), "
                               "%d, \"int\", \"%S_len\", "
                               "\"%S\", %d},\n",
                               st->name, field->name, FIELD_TYPE_INT,
                               field->name, st->name, 0);
            sstr_t tmp = sstr_dup(field->name);
            sstr_append_cstr(tmp, "_len");
            gen_hash_arr(st->name, tmp, param);
            sstr_free(tmp);
        }

        field = field->next;
    }
}

static void gen_code_offset_map(struct hash_map* struct_map, sstr_t source,
                                sstr_t header) {
    int total_fields = 0;
    hash_map_for_each(struct_map, count_fields_fd, &total_fields);
    sstr_printf_append(header, "#define JSON_FIELD_OFFSET_ITEM_SIZE %d\n",
                       total_fields + struct_map->size + 1);
    sstr_append_cstr(header,
                     "struct json_field_offset_item {\n"
                     "    int offset;\n"
                     "    int type_size;\n"
                     "    int field_type;\n"
                     "    char* field_type_name;\n"
                     "    char* field_name;\n"
                     "    char* struct_name;\n"
                     "    int is_array;\n"
                     "};\n\n");
    sstr_printf_append(
        source,
        "struct json_field_offset_item "
        "json_field_offset_item[JSON_FIELD_OFFSET_ITEM_SIZE] = {\n");

    struct gen_fields_list_fn_param param;
    param.header = header;
    param.source = source;
    param.hash_size = total_fields * 2 + 1;
    param.f_cnt = 0;
    param.hash_arr = (int*)malloc(sizeof(int) * param.hash_size);
    memset(param.hash_arr, -1, sizeof(int) * param.hash_size);
    hash_map_for_each(struct_map, gen_fields_list_fn, &param);
    sstr_append_cstr(source, "    {0, 0, 0, NULL, NULL, NULL, 0}};\n");

    sstr_printf_append(
        source, "int json_entry_hash_size = %d;\nint json_entry_hash[%d] = {",
        param.hash_size, param.hash_size);
    for (int i = 0; i < param.hash_size; i++) {
        char tmp_str[32];
        sprintf(tmp_str, i == 0 ? "%d" : ", %d", param.hash_arr[i]);
        sstr_append_cstr(source, tmp_str);
    }
    sstr_append_cstr(source, "};\n");
    free(param.hash_arr);
}

static void dummy_free(void* ptr) { (void)ptr; }

struct do_each_struct_gen_code_param {
    struct hash_map* dependency_map;
    sstr_t source;
    sstr_t header;
};

static void do_each_struct_gen_code(void* key, void* value, void* ptr) {
    sstr_t k = (sstr_t)key;
    struct struct_container* v = (struct struct_container*)value;
    struct do_each_struct_gen_code_param* param =
        (struct do_each_struct_gen_code_param*)ptr;
    struct hash_map* dep_map = param->dependency_map;

    void* dv = NULL;

    int r = hash_map_find(dep_map, k, &dv);
    if (r == HASH_MAP_OK) {
        return;
    }

    struct struct_field* iter = v->fields;
    while (iter) {
        if (iter->type == FIELD_TYPE_STRUCT) {
            int r = hash_map_find(dep_map, iter->type_name, &dv);
            if (r != HASH_MAP_OK) {
                return;
            }
        }
        iter = iter->next;
    }
    // all dependency is resolved
    gen_code_struct(v, param->source, param->header);
    hash_map_insert(dep_map, sstr_dup(k), NULL);
}

int gencode_head_guard_begin(sstr_t head) {
    sstr_append_cstr(head,
                     "#ifndef JSON_GEN_C_H__\n#define JSON_GEN_C_H__\n\n");
    sstr_append_cstr(head, "#include \"sstr.h\"\n");
    sstr_append_cstr(head, "#ifdef __cplusplus\n");
    sstr_append_cstr(head, "extern \"C\" {\n");
    sstr_append_cstr(head, "#endif\n\n");
    sstr_append_cstr(
        head, "int json_marshal_array_int(int*obj, int len, sstr_t out);\n");
    sstr_append_cstr(
        head, "int json_marshal_array_long(long*obj, int len, sstr_t out);\n");
    sstr_append_cstr(
        head,
        "int json_marshal_array_float(float*obj, int len, sstr_t out);\n");
    sstr_append_cstr(
        head,
        "int json_marshal_array_double(double*obj, int len, sstr_t out);\n");
    sstr_append_cstr(
        head,
        "int json_marshal_array_sstr_t(sstr_t*obj, int len, sstr_t out);\n\n");

    return 0;
}

int gencode_head_guard_end(sstr_t head) {
    sstr_append_of(head, codes_json_parse_h, (size_t)codes_json_parse_h_len);

    sstr_printf_append(head, "\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n\n");

    return 0;
}

int gencode_source_begin(sstr_t source) {
    sstr_printf_append(source,
                       "#include \"%s\"\n\n#include <stdio.h>\n"
                       "#include <malloc.h>\n\n",
                       OUTPUT_H_FILENAME);
    gen_code_scalar_marshal_array(source);
    return 0;
}

int gencode_source_end(sstr_t source) {
    sstr_append_of(source, codes_json_parse_c, (size_t)codes_json_parse_c_len);
    return 0;
}

int gencode_source(struct hash_map* struct_map, sstr_t source, sstr_t header) {
    struct hash_map* dependency_map =
        hash_map_new(DEPENDENCY_HASH_MAP_BUCKET_SIZE, sstr_key_hash,
                     sstr_key_cmp, sstr_key_free, dummy_free);
    if (dependency_map == NULL) {
        return -1;
    }
    int i;
    struct do_each_struct_gen_code_param param;
    param.dependency_map = dependency_map;
    param.source = source;
    param.header = header;
    gencode_head_guard_begin(header);
    gencode_source_begin(source);

    for (i = 0; i < struct_map->size; ++i) {
        hash_map_for_each(struct_map, do_each_struct_gen_code, &param);
        if (struct_map->size == dependency_map->size) {
            break;
        }
    }
    if (struct_map->size != dependency_map->size) {
        printf("msize = %d, d size=%d\n", struct_map->size,
               dependency_map->size);
        fprintf(stderr, "struct dependency circle detected\n");
        hash_map_free(dependency_map);
        return -1;
    }

    gen_code_offset_map(struct_map, source, header);
    gencode_head_guard_end(header);
    gencode_source_end(source);

    hash_map_free(dependency_map);
    return 0;
}
