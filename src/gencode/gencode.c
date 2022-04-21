#include "gencode/gencode.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"

#define DEPENDENCY_HASH_MAP_BUCKET_SIZE 4096

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

// header file for a single struct.
static void gen_code_struct_header(struct struct_container* st, sstr_t header) {
    // struct definitions
    sstr_append_cstr(header, "struct ");
    sstr_append(header, st->name);
    sstr_append_cstr(header, " {\n");
    struct struct_field* field = st->fields;
    while (field) {
        // fields
        sstr_append_cstr(header, "    ");
        if (field->type == FIELD_TYPE_STRUCT) {
            sstr_append_cstr(header, "struct ");
        }
        sstr_append(header, field->type_name);
        if (field->is_array) {
            // if it's an array, set type to pointer
            sstr_append_cstr(header, "*");
        }
        sstr_append_cstr(header, " ");
        sstr_append(header, field->name);
        sstr_append_cstr(header, ";\n");

        if (field->is_array) {
            // if it's an array, add a field_name_len integer field
            // to denote the size of array.
            sstr_printf_append(header, "    int %S_len;\n", field->name);
        }

        field = field->next;
    }
    sstr_append_cstr(header, "};\n\n");

    // init/uninit functions
    sstr_printf_append(header,
                       "/**\n"
                       " * @brief initialize function for struct %S\n"
                       " * it set all fields of obj to 0.\n"
                       " *\n"
                       " * @param obj the struct object to be initialized\n"
                       " */\n",
                       st->name);
    sstr_printf_append(header, "int %S_init(struct %S* obj);\n", st->name,
                       st->name);

    sstr_printf_append(
        header,
        "/**\n"
        " * @brief uninitialize function for struct %S\n"
        " * it set all fields of obj to 0, and free all\n"
        " * dynamically allocated memory of fields inside recursively.\n"
        " */\n",
        st->name);
    sstr_printf_append(header, "int %S_clear(struct %S* obj);\n", st->name,
                       st->name);

    // marshal/unmarshal functions
    sstr_printf_append(
        header,
        "/**\n"
        " * @brief Convert (marshal) struct %S to a well indented json "
        "string.\n"
        " * @param obj the struct object to be marshaled\n"
        " * @param indent the indentation spaces of the output json string\n"
        " * @param curindent the current indentation before call this "
        "function,\n"
        " * set it to 0 if for normal purpose.\n"
        " * @param out the output json string.\n"
        " */\n",
        st->name);
    sstr_printf_append(header,
                       "int json_marshal_indent_%S(struct %S* obj, int indent, "
                       "int curindent, sstr_t out);\n",
                       st->name, st->name);

    sstr_printf_append(
        header,
        "/**\n"
        " * @brief Convert (marshal) struct %S to a not indented json "
        "string.\n"
        " * @param obj the struct object to be marshaled\n"
        " * @param out the output json string.\n"
        " */\n",
        st->name);
    sstr_printf_append(header,
                       "#define json_marshal_%S(obj, out) "
                       "json_marshal_indent_%S(obj, 0, 0, out)\n",
                       st->name, st->name);

    sstr_printf_append(
        header,
        "/**\n"
        " * @brief Convert (marshal) an array of struct %S to a well indented "
        "json string.\n"
        " * @param obj the array of struct object to be marshaled\n"
        " * @param indent the indentation spaces of the output json string\n"
        " * @param curindent the current indentation before call this "
        "function,\n"
        " * set it to 0 if for normal purpose.\n"
        " * @param out the output json string.\n"
        " */\n",
        st->name);
    sstr_printf_append(header,
                       "int json_marshal_array_indent_%S(struct %S* obj, int "
                       "len, int indent, int curindent, sstr_t out);\n",
                       st->name, st->name);
    sstr_printf_append(
        header,
        "/**\n"
        " * @brief Convert (marshal) array of struct %S to a (un)indented json "
        "string.\n"
        " * @param obj the struct object to be marshaled\n"
        " * @param out the output json string.\n"
        " */\n",
        st->name);
    sstr_printf_append(header,
                       "#define json_marshal_array_%S(obj, len, out) "
                       "json_marshal_array_indent_%S(obj, len, 0, 0, out)\n",
                       st->name, st->name);

    sstr_printf_append(header,
                       "/**\n"
                       " * @brief Convert (unmarshal) a json string to an "
                       "object of struct %S.\n"
                       " * @param in the input json string.\n"
                       " * @param obj the output struct object.\n"
                       " */\n",
                       st->name);
    sstr_printf_append(header,
                       "int json_unmarshal_%S(sstr_t in, struct %S* obj);\n",
                       st->name, st->name);
    sstr_printf_append(header,
                       "/**\n"
                       " * @brief Convert (unmarshal) a json string to an "
                       "object array of struct %S.\n"
                       " * @param in the input json string.\n"
                       " * @param obj the output struct object.\n"
                       " * @param len the output length of the array.\n"
                       " */\n",
                       st->name);
    sstr_printf_append(header,
                       "int json_unmarshal_array_%S(sstr_t in, struct %S** "
                       "obj, int* len);\n\n",
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

// marshal function for a single struct
static void gen_code_struct_marshal_struct(struct struct_container* st,
                                           sstr_t source) {
    sstr_printf_append(source,
                       "int json_marshal_indent_%S(struct %S* obj, int indent, "
                       "int curindent, sstr_t out) {\n",
                       st->name, st->name);
    sstr_append_cstr(source,
                     "    char tmp_cstr[64];\n    (void)tmp_cstr;\n"
                     "    if (indent && sstr_length(out) && "
                     "sstr_cstr(out)[sstr_length(out)-1] != ':') {\n"
                     "        sstr_append_indent(out, curindent);\n"
                     "    }\n"
                     "    sstr_append_cstr(out, \"{\");\n"
                     "    sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                     "    curindent += indent;\n");

    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        sstr_append_cstr(source, "    sstr_append_indent(out, curindent);\n");
        sstr_printf_append(source,
                           "    sstr_append_cstr(out, \"\\\"%S\\\":\");\n",
                           field->name);

        if (field->is_array) {
            sstr_printf_append(source,
                               "    json_marshal_array_indent_%S(obj->%S, "
                               "obj->%S_len, indent, curindent, out);\n",
                               field->type_name, field->name, field->name);
            if (field->next != NULL) {
                sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
            }
            sstr_append_cstr(
                source, "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
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
                                   "    json_marshal_indent_%S(&obj->%S, "
                                   "indent, curindent, out);\n",
                                   field->type_name, field->name);
                break;
        }

        if (field->next == NULL) {
        } else {
            sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
        }
        sstr_append_cstr(source,
                         "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
    }
    sstr_append_cstr(source, "    curindent -= indent;\n");

    sstr_append_cstr(source,
                     "    sstr_append_indent(out, curindent);\n"
                     "    sstr_append_of(out, \"}\", 1);\n"
                     "    return 0;\n}\n\n");
}

static void gen_code_struct_marshal_array(struct struct_container* st,
                                          sstr_t source) {
    sstr_printf_append(source,
                       "int json_marshal_array_indent_%S(struct %S* obj, int "
                       "len, int indent, int curindent, sstr_t out) {\n",
                       st->name, st->name);

    sstr_append_cstr(source,
                     "    int i;\n"
                     "    sstr_append_of(out, \"[\", 1);\n"
                     "    sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                     "    curindent += indent;\n"
                     "    for (i = 0; i < len; i++) {\n");
    sstr_printf_append(
        source,
        "        json_marshal_indent_%S(&obj[i], indent, curindent, out);\n",
        st->name);
    sstr_append_cstr(source,
                     "        if (i < len - 1) {\n"
                     "            sstr_append_cstr(out, \",\");\n"
                     "        }\n"
                     "        sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                     "    }\n"
                     "    curindent -= indent;\n"
                     "    sstr_append_indent(out, curindent);\n"
                     "    sstr_append_of(out, \"]\", 1);\n"
                     "\n    return 0;\n}\n\n");
}

static void gen_code_scalar_marshal_array(sstr_t source) {
    // NOTE: move to json_parse.h
    (void)source;
}

// init functions for a single struct
static void gen_code_struct_init(struct struct_container* st, sstr_t source) {
    sstr_printf_append(source, "int %S_init(struct %S*obj) {\n", st->name,
                       st->name);
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        if (field->is_array) {
            // if array, an extra integer field XXX_len denoting the size
            // of the array.
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

// clear function for a single struct
static void gen_code_struct_clear(struct struct_container* st, sstr_t source) {
    sstr_printf_append(source, "int %S_clear(struct %S*obj) {\n", st->name,
                       st->name);
    int have_i = 0;
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        if (field->is_array) {
            // free array, and set XXX_len=0
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
            case FIELD_TYPE_LONG:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                break;
            case FIELD_TYPE_FLOAT:
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

// generate the struct codes
static void gen_code_struct(struct struct_container* st, sstr_t source,
                            sstr_t header) {
    // type definitions and function declares.
    gen_code_struct_header(st, header);
    // XXX_init()
    gen_code_struct_init(st, source);
    // XXX_clear()
    gen_code_struct_clear(st, source);
    // json_marshal_XXX()
    gen_code_struct_marshal_struct(st, source);
    // json_unmarshal_XXX()
    gen_code_struct_unmarshal_struct(st, source);
    // json_unmarshal_array_XXX()
    gen_code_struct_unmarshal_array_struct(st, source);
    // json_marshal_array_XXX()
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

    sstr_printf_append(param->source,
                       "    {0, sizeof(struct %S), %d, \"\", \"\", \"%S\", 0, "
                       "%S_clear},\n",
                       st->name, FIELD_TYPE_STRUCT, st->name, st->name,
                       st->name);
    sstr_t empty_s = sstr_new();
    gen_hash_arr(st->name, empty_s, param);
    sstr_free(empty_s);

    while (field) {
        // field list
        if (field->type == FIELD_TYPE_STRUCT) {
            sstr_printf_append(param->source,
                               "    {offsetof(struct %S, %S), sizeof(struct "
                               "%S), %d, \"%S\", \"%S\", "
                               "\"%S\", %d, NULL},\n",
                               st->name, field->name, field->type_name,
                               field->type, field->type_name, field->name,
                               st->name, field->is_array);
            gen_hash_arr(st->name, field->name, param);
        } else {
            sstr_printf_append(
                param->source,
                "    {offsetof(struct %S, %S), sizeof(%S), %d, \"%S\", \"%S\", "
                "\"%S\", %d, NULL},\n",
                st->name, field->name, field->type_name, field->type,
                field->type_name, field->name, st->name, field->is_array);
            gen_hash_arr(st->name, field->name, param);
        }
        if (field->is_array) {
            sstr_printf_append(param->source,
                               "    {offsetof(struct %S, %S_len), sizeof(int), "
                               "%d, \"int\", \"%S_len\", "
                               "\"%S\", %d, NULL},\n",
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

/*
    generate hash map of struct like:

    hash(structname, fieldname) --> index of json_field_offset_item
                                      |
                                -------
                                |
                                v
    json_field_offset_item:  [] [index of item] [] [] [] []
                                       |
                                       -----
                                           |
                                           v
    json_field_offset_item: [item] [item] [item] item
                                          |:
                                            {
                                                offset, type_size ...
                                            }
*/
static void gen_code_offset_map(struct hash_map* struct_map, sstr_t source,
                                sstr_t header) {
    int total_fields = 0;
    hash_map_for_each(struct_map, count_fields_fd, &total_fields);
    sstr_printf_append(source, "#define JSON_FIELD_OFFSET_ITEM_SIZE %d\n",
                       total_fields + struct_map->size + 1);
    sstr_append_cstr(source,
                     "typedef void (*clear_st_fn_t)(void*);\n"
                     "struct json_field_offset_item {\n"
                     "    int offset;\n"
                     "    int type_size;\n"
                     "    int field_type;\n"
                     "    char* field_type_name;\n"
                     "    char* field_name;\n"
                     "    char* struct_name;\n"
                     "    int is_array;\n"
                     "    void* clear_st_fn;"
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
    sstr_append_cstr(source, "    {0, 0, 0, NULL, NULL, NULL, 0, NULL}};\n");

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

// generate code for each struct
static void do_each_struct_gen_code(void* key, void* value, void* ptr) {
    sstr_t k = (sstr_t)key;
    struct struct_container* v = (struct struct_container*)value;
    struct do_each_struct_gen_code_param* param =
        (struct do_each_struct_gen_code_param*)ptr;
    struct hash_map* dep_map = param->dependency_map;

    void* dv = NULL;

    // jsut return if the struct is already generated.
    int r = hash_map_find(dep_map, k, &dv);
    if (r == HASH_MAP_OK) {
        return;
    }

    // for each field, check if it is a struct type, and already
    // inserted into dependency map, if not, we cannot generate code
    // for this struct, just return.
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

    // now the struct is generated, we can insert it into dependency map
    // to avoid generating it again.
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
        head,
        "/**\n"
        " * @brief Convert (marshal) array of int to indented json string.\n"
        " * @param obj The array of ints.\n"
        " * @param len length of array obj.\n"
        " * @indent The indent space of json string.\n"
        " * @curindent The current indent space of json string before calling "
        "this function, \n"
        " * set to 0 if you don't kown what it means.\n"
        " * @param out The output json string.\n"
        " */\n"
        "int json_marshal_array_indent_int(int* obj, int len, int indent, int "
        "curindent, sstr_t out);\n"
        "/**\n"
        " * @brief Convert (marshal) array of longs to indented json string.\n"
        " * @param obj The array of longs.\n"
        " * @param len length of array obj.\n"
        " * @indent The indent space of json string.\n"
        " * @curindent The current indent space of json string before calling "
        "this function, \n"
        " * set to 0 if you don't kown what it means.\n"
        " * @param out The output json string.\n"
        " */\n"
        "int json_marshal_array_indent_long(long* obj, int len, int indent, "
        "int "
        "curindent, sstr_t out);\n"
        "/**\n"
        " * @brief Convert (marshal) array of floats to indented json string.\n"
        " * @param obj The array of floats.\n"
        " * @param len length of array obj.\n"
        " * @indent The indent space of json string.\n"
        " * @curindent The current indent space of json string before calling "
        "this function, \n"
        " * set to 0 if you don't kown what it means.\n"
        " * @param out The output json string.\n"
        " */\n"
        "int json_marshal_array_indent_float(float* obj, int len, int indent, "
        "int curindent, sstr_t out);\n"
        "/**\n"
        " * @brief Convert (marshal) array of double to indented json string.\n"
        " * @param obj The array of doubles.\n"
        " * @param len length of array obj.\n"
        " * @indent The indent space of json string.\n"
        " * @curindent The current indent space of json string before calling "
        "this function, \n"
        " * set to 0 if you don't kown what it means.\n"
        " * @param out The output json string.\n"
        " */\n"
        "int json_marshal_array_indent_double(double* obj, int len, int "
        "indent, "
        "int curindent, sstr_t "
        "out);\n"
        "/**\n"
        " * @brief Convert (marshal) array of sstr_t to indented json string.\n"
        " * @param obj The array of sstr_t's.\n"
        " * @param len length of array obj.\n"
        " * @indent The indent space of json string.\n"
        " * @curindent The current indent space of json string before calling "
        "this function, \n"
        " * set to 0 if you don't kown what it means.\n"
        " * @param out The output json string.\n"
        " */\n"
        "int json_marshal_array_indent_sstr_t(sstr_t* obj, int len, int "
        "indent, "
        "int curindent, sstr_t "
        "out);\n\n"
        "#define json_marshal_array_int(obj, len, out) "
        "json_marshal_array_indent_int(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_long(obj, len, out) "
        "json_marshal_array_indent_long(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_float(obj, len, out) "
        "json_marshal_array_indent_float(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_double(obj, len, out) "
        "json_marshal_array_indent_double(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_sstr_t(obj, len, out) "
        "json_marshal_array_indent_sstr_t(obj, len, 0, 0, out)\n\n"

        "/**\n"
        " * @brief Convert (unmarshal) json string to array of int.\n"
        " * @param content The json string.\n"
        " * @param ptr A pointer to array of int.\n"
        " * @param len A pointer to int variable to store length of array.\n"
        " */\n"
        "int json_unmarshal_array_int(sstr_t content, int** ptr, int* "
        "len);\n"
        "/**\n"
        " * @brief Convert (unmarshal) json string to array of long.\n"
        " * @param content The json string.\n"
        " * @param ptr A pointer to array of long.\n"
        " * @param len A pointer to int variable to store length of array.\n"
        " */\n"
        "int json_unmarshal_array_long(sstr_t content, long** ptr, int* "
        "len);\n"
        "/**\n"
        " * @brief Convert (unmarshal) json string to array of double.\n"
        " * @param content The json string.\n"
        " * @param ptr A pointer to array of double.\n"
        " * @param len A pointer to int variable to store length of array.\n"
        " */\n"
        "int json_unmarshal_array_double(sstr_t content, double** ptr, "
        "int* "
        "len);\n"
        "/**\n"
        " * @brief Convert (unmarshal) json string to array of float.\n"
        " * @param content The json string.\n"
        " * @param ptr A pointer to array of floats.\n"
        " * @param len A pointer to int variable to store length of array.\n"
        " */\n"
        "int json_unmarshal_array_float(sstr_t content, float** ptr, "
        "int* "
        "len);\n"
        "/**\n"
        " * @brief Convert (unmarshal) json string to array of sstr_t.\n"
        " * @param content The json string.\n"
        " * @param ptr A pointer to array of sstr_t.\n"
        " * @param len A pointer to int variable to store length of array.\n"
        " */\n"
        "int json_unmarshal_array_sstr_t(sstr_t content, sstr_t** ptr, "
        "int* "
        "len);\n\n");

    return 0;
}

int gencode_head_guard_end(sstr_t head) {
    sstr_printf_append(head, "\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n\n");

    return 0;
}

#include "extra_codes.inc"
int gencode_source_begin(sstr_t source) {
    sstr_printf_append(source,
                       "#include \"%s\"\n\n#include <stdio.h>\n"
                       "#include <malloc.h>\n\n",
                       OUTPUT_H_FILENAME);
    sstr_append_of(source, codes_json_parse_h, (size_t)codes_json_parse_h_len);
    gen_code_scalar_marshal_array(source);
    return 0;
}

int gencode_source_end(sstr_t source) {
    sstr_append_of(source, codes_json_parse_c, (size_t)codes_json_parse_c_len);
    return 0;
}

int gencode_source(struct hash_map* struct_map, sstr_t source, sstr_t header) {
    // to ensure the order struct definition on header file, we use a hash map
    // to store the struct names that already defined in header file.
    // if a struct is not in the hash map, we test all field of it, if any field
    // is a struct and not in the hash map, we skip it to the next loop. If all
    // fields in the struct are already in the hash map, we generate the code of
    // the struct.
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

    // generate header guards and all common functions declarations.
    gencode_head_guard_begin(header);
    // includes, and all common functions, scalar type parsing codes.
    gencode_source_begin(source);

    // generate a hashmap to store the structs name, fields, types, and the
    // offset of each field on the struct. the json parser codes need this
    // information to store the parsed data.
    gen_code_offset_map(struct_map, source, header);

    // Think about the worst case, we only have one struct defined on each loop,
    // then we must have loop through struct_map the times same as the number of
    // structs.
    for (i = 0; i < struct_map->size; ++i) {
        // for each struct, find a struct that not defined and all it's fields
        // are already defined, generate the codes for it.
        hash_map_for_each(struct_map, do_each_struct_gen_code, &param);
        if (struct_map->size == dependency_map->size) {
            break;
        }
    }
    // if all struct are already defined, the dependency_map's size
    // should be equal to struct_map's size.
    // if not, we have a circular dependency.
    if (struct_map->size != dependency_map->size) {
        printf("msize = %d, d size=%d\n", struct_map->size,
               dependency_map->size);
        fprintf(stderr, "struct dependency circle detected\n");
        hash_map_free(dependency_map);
        return -1;
    }

    gencode_head_guard_end(header);
    // append json_parse.c
    gencode_source_end(source);

    hash_map_free(dependency_map);
    return 0;
}
