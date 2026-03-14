#include "gencode/gencode.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"
#include "utils/hash.h"

#define DEPENDENCY_HASH_MAP_BUCKET_SIZE 4096

// Get the effective JSON key name for a field (json_name if set, otherwise name)
#define JSON_KEY(f) ((f)->json_name ? (f)->json_name : (f)->name)

// Get the C value type string for a map field's value.
// For struct values, prepends "struct ".
static void map_c_value_type(struct struct_field* field, sstr_t out) {
    if (field->map_value_type == FIELD_TYPE_STRUCT) {
        sstr_append_cstr(out, "struct ");
        sstr_append(out, field->map_value_type_name);
    } else if (field->map_value_type == FIELD_TYPE_ENUM ||
               field->map_value_type == FIELD_TYPE_BOOL) {
        sstr_append_cstr(out, "int");
    } else {
        sstr_append(out, field->map_value_type_name);
    }
}

// Get the suffix name for map entry/container structs (e.g. "int", "sstr_t", "Person").
// For bool and enum, returns "int" since they share int representation.
static const char* map_suffix(struct struct_field* field) {
    if (field->map_value_type == FIELD_TYPE_ENUM ||
        field->map_value_type == FIELD_TYPE_BOOL) {
        return "int";
    }
    return sstr_cstr(field->map_value_type_name);
}

inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int h = 0xbc9f1d34;
    h = hash_murmur(sstr_cstr(key1), sstr_length(key1), h);
    h = hash_murmur("#", 1, h);
    h = hash_murmur(sstr_cstr(key2), sstr_length(key2), h);
    return h;
}

static void gen_code_struct_marshal_array(struct struct_container* st,
                                          sstr_t source);

// header file for a single struct.
static void gen_code_struct_header(struct struct_container* st, sstr_t header) {
    // emit map entry/container struct definitions (idempotent via #ifndef guards)
    struct struct_field* mf = st->fields;
    while (mf) {
        if (mf->type == FIELD_TYPE_MAP) {
            const char* sfx = map_suffix(mf);
            sstr_printf_append(header,
                "#ifndef JSON_MAP_%s_DEFINED\n"
                "#define JSON_MAP_%s_DEFINED\n"
                "struct json_map_entry_%s { sstr_t key; ",
                sfx, sfx, sfx);
            sstr_t vtype = sstr_new();
            map_c_value_type(mf, vtype);
            sstr_append(header, vtype);
            sstr_free(vtype);
            sstr_printf_append(header,
                " value; };\n"
                "struct json_map_%s { struct json_map_entry_%s* entries; int len; };\n"
                "#endif\n\n",
                sfx, sfx);
        }
        mf = mf->next;
    }

    // struct definitions
    sstr_append_cstr(header, "struct ");
    sstr_append(header, st->name);
    sstr_append_cstr(header, " {\n");
    struct struct_field* field = st->fields;
    while (field) {
        // fields
        sstr_append_cstr(header, "    ");
        if (field->type == FIELD_TYPE_MAP) {
            const char* sfx = map_suffix(field);
            if (field->is_array && field->array_size == 0) {
                sstr_printf_append(header, "struct json_map_%s* %S;\n",
                                   sfx, field->name);
                sstr_printf_append(header, "    int %S_len;\n", field->name);
            } else {
                sstr_printf_append(header, "struct json_map_%s %S;\n",
                                   sfx, field->name);
            }
            if (field->is_optional || field->is_nullable) {
                sstr_printf_append(header, "    bool has_%S;\n", field->name);
            }
            field = field->next;
            continue;
        }
        if (field->type == FIELD_TYPE_STRUCT) {
            sstr_append_cstr(header, "struct ");
        }
        if (field->type == FIELD_TYPE_ENUM) {
            // enums are stored as int
            sstr_append_cstr(header, "int");
        } else {
            sstr_append(header, field->type_name);
        }
        if (field->is_array && field->array_size > 0) {
            // fixed-size array: type name[N];
        } else if (field->is_array) {
            // dynamic array: set type to pointer
            sstr_append_cstr(header, "*");
        }
        sstr_append_cstr(header, " ");
        sstr_append(header, field->name);
        if (field->is_array && field->array_size > 0) {
            sstr_printf_append(header, "[%d]", field->array_size);
        }
        sstr_append_cstr(header, ";\n");

        if (field->is_optional || field->is_nullable) {
            sstr_printf_append(header, "    bool has_%S;\n", field->name);
        }

        if (field->is_array && field->array_size == 0) {
            // dynamic array: add a field_name_len integer field
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
        "#endif\n");
    sstr_printf_append(source, "        %S_clear(obj);\n", st->name);
    sstr_append_cstr(source,
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
    sstr_printf_append(source,
                       "    if (r < 0) {\n"
                       "#ifdef JSON_DEBUG\n"
                       "        printf(\"ERROR: %%s\", sstr_cstr(txt));\n"
                       "#endif\n"
                       "        int i;\n"
                       "        for (i = 0; i < *len; ++i) {\n"
                       "            %S_clear(&(*obj)[i]);\n"
                       "        }\n"
                       "    JGENC_FREE(*obj);\n"
                       "    *obj = NULL;\n"
                       "    *len = 0;\n"
                       "    }\n",
                       st->name);

    sstr_append_cstr(source, "    sstr_free(txt);\n");
    sstr_append_cstr(source, "    return r;\n");
    sstr_append_cstr(source, "}\n\n");
}

// Helper: emit code to marshal a single map entry value.
// `field_name` is the C field name, `val_suffix` is appended to obj->field_name
// to form the value expression (e.g. ".entries[_mk].value").
static void gen_marshal_map_value(struct struct_field* field,
                                  const char* field_name,
                                  const char* val_suffix,
                                  sstr_t source) {
    // expression prefix: "obj-><field_name><val_suffix>"
    switch (field->map_value_type) {
        case FIELD_TYPE_INT:
        case FIELD_TYPE_BOOL:
            sstr_printf_append(source,
                "            sstr_append_int_str(out, obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_LONG:
            sstr_printf_append(source,
                "            sstr_append_long_str(out, obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_INT8:
        case FIELD_TYPE_INT16:
        case FIELD_TYPE_INT32:
        case FIELD_TYPE_UINT8:
        case FIELD_TYPE_UINT16:
            sstr_printf_append(source,
                "            sstr_append_int_str(out, (int)obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_INT64:
            sstr_printf_append(source,
                "            sstr_append_long_str(out, (long)obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_UINT32:
            sstr_printf_append(source,
                "            sstr_append_uint32_str(out, obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_UINT64:
            sstr_printf_append(source,
                "            sstr_append_uint64_str(out, obj->%s%s);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_FLOAT:
            sstr_printf_append(source,
                "            sstr_append_float_str(out, obj->%s%s, -1);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_DOUBLE:
            sstr_printf_append(source,
                "            sstr_append_double_str(out, obj->%s%s, -1);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_SSTR:
            sstr_printf_append(source,
                "            sstr_append_of(out, \"\\\"\", 1);\n"
                "            sstr_json_escape_string_append(out, obj->%s%s);\n"
                "            sstr_append_of(out, \"\\\"\", 1);\n",
                field_name, val_suffix);
            break;
        case FIELD_TYPE_STRUCT:
            sstr_printf_append(source,
                "            json_marshal_indent_%S(&obj->%s%s, indent, curindent, out);\n",
                field->map_value_type_name, field_name, val_suffix);
            break;
        case FIELD_TYPE_ENUM:
            sstr_printf_append(source,
                "            if (obj->%s%s >= 0 && obj->%s%s < %S_enum_count) {\n"
                "                sstr_append_of(out, \"\\\"\", 1);\n"
                "                sstr_append_cstr(out, %S_enum_strings[obj->%s%s]);\n"
                "                sstr_append_of(out, \"\\\"\", 1);\n"
                "            } else {\n"
                "                sstr_append_int_str(out, obj->%s%s);\n"
                "            }\n",
                field_name, val_suffix, field_name, val_suffix,
                field->map_value_type_name,
                field->map_value_type_name, field_name, val_suffix,
                field_name, val_suffix);
            break;
    }
}

// marshal function for a single struct
static void gen_code_struct_marshal_struct(struct struct_container* st,
                                           sstr_t source) {
    int has_optional = 0;
    {
        struct struct_field* f = st->fields;
        for (; f; f = f->next) {
            if (f->is_optional || f->is_nullable) { has_optional = 1; break; }
        }
    }

    sstr_printf_append(source,
                       "int json_marshal_indent_%S(struct %S* obj, int indent, "
                       "int curindent, sstr_t out) {\n",
                       st->name, st->name);
    sstr_append_cstr(source,
                     "    char tmp_cstr[64];\n    (void)tmp_cstr;\n");
    if (has_optional) {
        sstr_append_cstr(source, "    int _first = 1;\n");
    }
    sstr_append_cstr(source,
                     "    if (indent && sstr_length(out) && "
                     "sstr_cstr(out)[sstr_length(out)-1] != ':') {\n"
                     "        sstr_append_indent(out, curindent);\n"
                     "    }\n"
                     "    sstr_append_cstr(out, \"{\");\n");
    if (!has_optional) {
        sstr_append_cstr(source,
                         "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
    }
    sstr_append_cstr(source, "    curindent += indent;\n");

    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        // When struct has optional fields, use comma-before pattern
        if (has_optional) {
            if (field->is_optional) {
                sstr_printf_append(source,
                    "    if (obj->has_%S) {\n", field->name);
            }
            sstr_append_cstr(source,
                "    if (!_first) { sstr_append_cstr(out, \",\"); }\n"
                "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
        }
        sstr_append_cstr(source, "    sstr_append_indent(out, curindent);\n");
        sstr_printf_append(source,
                           "    sstr_append_cstr(out, \"\\\"%S\\\":\");\n",
                           JSON_KEY(field));
        // Nullable-only: wrap value in null check
        if (has_optional && field->is_nullable && !field->is_optional) {
            sstr_printf_append(source,
                "    if (!obj->has_%S) {\n"
                "        sstr_append_cstr(out, \"null\");\n"
                "    } else {\n", field->name);
        }

        if (field->type == FIELD_TYPE_MAP) {
            // Marshal map field(s) as JSON object(s)
            if (field->is_array && field->array_size == 0) {
                // array of maps: output [{ ... }, { ... }]
                sstr_append_cstr(source,
                    "    sstr_append_of(out, \"[\", 1);\n"
                    "    sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "    curindent += indent;\n"
                    "    { int _aj;\n");
                sstr_printf_append(source,
                    "    for (_aj = 0; _aj < obj->%S_len; _aj++) {\n"
                    "        sstr_append_indent(out, curindent);\n"
                    "        sstr_append_of(out, \"{\", 1);\n"
                    "        sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "        curindent += indent;\n"
                    "        { int _mk;\n",
                    field->name);
                sstr_printf_append(source,
                    "        for (_mk = 0; _mk < obj->%S[_aj].len; _mk++) {\n"
                    "            sstr_append_indent(out, curindent);\n"
                    "            sstr_append_of(out, \"\\\"\", 1);\n",
                    field->name);
                sstr_printf_append(source,
                    "            sstr_json_escape_string_append(out, obj->%S[_aj].entries[_mk].key);\n"
                    "            sstr_append_cstr(out, \"\\\":\");\n",
                    field->name);
                // emit value marshal
                gen_marshal_map_value(field,
                    sstr_cstr(field->name), "[_aj].entries[_mk].value",
                    source);
                sstr_printf_append(source,
                    "            if (_mk < obj->%S[_aj].len - 1) sstr_append_cstr(out, \",\");\n"
                    "            sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "        } }\n"
                    "        curindent -= indent;\n"
                    "        sstr_append_indent(out, curindent);\n"
                    "        sstr_append_of(out, \"}\", 1);\n",
                    field->name);
                sstr_printf_append(source,
                    "        if (_aj < obj->%S_len - 1) sstr_append_cstr(out, \",\");\n"
                    "        sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "    } }\n"
                    "    curindent -= indent;\n"
                    "    sstr_append_indent(out, curindent);\n"
                    "    sstr_append_of(out, \"]\", 1);\n",
                    field->name);
            } else {
                // scalar map: output { "k1":v1, "k2":v2 }
                sstr_append_cstr(source,
                    "    sstr_append_of(out, \"{\", 1);\n"
                    "    sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "    curindent += indent;\n"
                    "    { int _mk;\n");
                sstr_printf_append(source,
                    "    for (_mk = 0; _mk < obj->%S.len; _mk++) {\n"
                    "        sstr_append_indent(out, curindent);\n"
                    "        sstr_append_of(out, \"\\\"\", 1);\n",
                    field->name);
                sstr_printf_append(source,
                    "        sstr_json_escape_string_append(out, obj->%S.entries[_mk].key);\n"
                    "        sstr_append_cstr(out, \"\\\":\");\n",
                    field->name);
                // emit value marshal
                gen_marshal_map_value(field,
                    sstr_cstr(field->name), ".entries[_mk].value",
                    source);
                sstr_printf_append(source,
                    "        if (_mk < obj->%S.len - 1) sstr_append_cstr(out, \",\");\n"
                    "        sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "    } }\n"
                    "    curindent -= indent;\n"
                    "    sstr_append_indent(out, curindent);\n"
                    "    sstr_append_of(out, \"}\", 1);\n",
                    field->name);
            }
            if (has_optional) {
                if (field->is_nullable && !field->is_optional) {
                    sstr_append_cstr(source, "    }\n");
                }
                sstr_append_cstr(source, "    _first = 0;\n");
                if (field->is_optional) {
                    sstr_append_cstr(source, "    }\n");
                }
            } else {
                if (field->next != NULL) {
                    sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
                }
                sstr_append_cstr(source,
                    "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
            }
            continue;
        }

        if (field->is_array) {
            if (field->type == FIELD_TYPE_ENUM) {
                // enum arrays: marshal each element as a string
                sstr_append_cstr(source,
                    "    {\n"
                    "        int _ei;\n"
                    "        sstr_append_of(out, \"[\", 1);\n"
                    "        sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "        curindent += indent;\n");
                if (field->array_size > 0) {
                    sstr_printf_append(source,
                        "        for (_ei = 0; _ei < %d; _ei++) {\n",
                        field->array_size);
                } else {
                    sstr_printf_append(source,
                        "        for (_ei = 0; _ei < obj->%S_len; _ei++) {\n",
                        field->name);
                }
                sstr_printf_append(source,
                    "            sstr_append_indent(out, curindent);\n"
                    "            if (obj->%S[_ei] >= 0 && obj->%S[_ei] < %S_enum_count) {\n"
                    "                sstr_append_of(out, \"\\\"\", 1);\n"
                    "                sstr_append_cstr(out, %S_enum_strings[obj->%S[_ei]]);\n"
                    "                sstr_append_of(out, \"\\\"\", 1);\n"
                    "            } else {\n"
                    "                sstr_append_int_str(out, obj->%S[_ei]);\n"
                    "            }\n",
                    field->name, field->name, field->type_name,
                    field->type_name, field->name,
                    field->name);
                if (field->array_size > 0) {
                    sstr_printf_append(source,
                        "            if (_ei < %d - 1) {\n",
                        field->array_size);
                } else {
                    sstr_printf_append(source,
                        "            if (_ei < obj->%S_len - 1) {\n",
                        field->name);
                }
                sstr_append_cstr(source,
                    "                sstr_append_cstr(out, \",\");\n"
                    "            }\n"
                    "            sstr_append_of_if(out, \"\\n\", 1, indent);\n"
                    "        }\n");
                sstr_append_cstr(source,
                    "        curindent -= indent;\n"
                    "        sstr_append_indent(out, curindent);\n"
                    "        sstr_append_of(out, \"]\", 1);\n"
                    "    }\n");
            } else if (field->array_size > 0) {
                // fixed-size non-enum array
                sstr_printf_append(source,
                                   "    json_marshal_array_indent_%S(obj->%S, "
                                   "%d, indent, curindent, out);\n",
                                   field->type_name, field->name, field->array_size);
            } else {
                sstr_printf_append(source,
                                   "    json_marshal_array_indent_%S(obj->%S, "
                                   "obj->%S_len, indent, curindent, out);\n",
                                   field->type_name, field->name, field->name);
            }
            if (has_optional) {
                if (field->is_nullable && !field->is_optional) {
                    sstr_append_cstr(source, "    }\n");
                }
                sstr_append_cstr(source, "    _first = 0;\n");
                if (field->is_optional) {
                    sstr_append_cstr(source, "    }\n");
                }
            } else {
                if (field->next != NULL) {
                    sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
                }
                sstr_append_cstr(
                    source, "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
            }
            continue;
        }
        switch (field->type) {
            case FIELD_TYPE_BOOL:
                sstr_printf_append(source, "    if (obj->%S) {\n", field->name);
                sstr_append_cstr(source,
                                 "        sstr_append_cstr(out, \"true\");\n"
                                 "    } else {\n"
                                 "        sstr_append_cstr(out, \"false\");\n"
                                 "    }\n");
                break;
            case FIELD_TYPE_INT:
                sstr_printf_append(source,
                                   "    sstr_append_int_str(out, obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_LONG:
                sstr_printf_append(source,
                                   "    sstr_append_long_str(out, obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_INT8:
            case FIELD_TYPE_INT16:
            case FIELD_TYPE_INT32:
            case FIELD_TYPE_UINT8:
            case FIELD_TYPE_UINT16:
                sstr_printf_append(source,
                                   "    sstr_append_int_str(out, (int)obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_INT64:
                sstr_printf_append(source,
                                   "    sstr_append_long_str(out, (long)obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_UINT32:
                sstr_printf_append(source,
                                   "    sstr_append_uint32_str(out, obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_UINT64:
                sstr_printf_append(source,
                                   "    sstr_append_uint64_str(out, obj->%S);\n",
                                   field->name);
                break;
            case FIELD_TYPE_FLOAT:
                sstr_printf_append(source,
                                   "    sstr_append_float_str(out, obj->%S, -1);\n",
                                   field->name);
                break;
            case FIELD_TYPE_DOUBLE:
                sstr_printf_append(
                    source, "    sstr_append_double_str(out, obj->%S, -1);\n",
                    field->name);
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
            case FIELD_TYPE_ENUM:
                // marshal enum as string: look up the int value in the string array
                sstr_printf_append(source,
                                   "    if (obj->%S >= 0 && obj->%S < %S_enum_count) {\n"
                                   "        sstr_append_of(out, \"\\\"\", 1);\n"
                                   "        sstr_append_cstr(out, %S_enum_strings[obj->%S]);\n"
                                   "        sstr_append_of(out, \"\\\"\", 1);\n"
                                   "    } else {\n"
                                   "        sstr_append_int_str(out, obj->%S);\n"
                                   "    }\n",
                                   field->name, field->name, field->type_name,
                                   field->type_name, field->name,
                                   field->name);
                break;
        }

        if (has_optional) {
            if (field->is_nullable && !field->is_optional) {
                sstr_append_cstr(source, "    }\n");
            }
            sstr_append_cstr(source, "    _first = 0;\n");
            if (field->is_optional) {
                sstr_append_cstr(source, "    }\n");
            }
        } else {
            if (field->next == NULL) {
            } else {
                sstr_append_cstr(source, "    sstr_append_cstr(out, \",\");\n");
            }
            sstr_append_cstr(source,
                             "    sstr_append_of_if(out, \"\\n\", 1, indent);\n");
        }
    }
    if (has_optional) {
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
        if (field->is_optional || field->is_nullable) {
            sstr_printf_append(source, "    obj->has_%S = false;\n", field->name);
        }
        if (field->type == FIELD_TYPE_MAP) {
            if (field->is_array && field->array_size == 0) {
                sstr_printf_append(source, "    obj->%S = NULL;\n", field->name);
                sstr_printf_append(source, "    obj->%S_len = 0;\n", field->name);
            } else {
                sstr_printf_append(source, "    obj->%S.entries = NULL;\n", field->name);
                sstr_printf_append(source, "    obj->%S.len = 0;\n", field->name);
            }
            continue;
        }
        if (field->is_array && field->array_size > 0) {
            // fixed-size array initialization
            if (field->type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source, "    {\n        int _i;\n        for (_i = 0; _i < %d; _i++) {\n", field->array_size);
                sstr_printf_append(source, "            obj->%S[_i] = NULL;\n", field->name);
                sstr_append_cstr(source, "        }\n    }\n");
            } else if (field->type == FIELD_TYPE_STRUCT) {
                sstr_printf_append(source, "    {\n        int _i;\n        for (_i = 0; _i < %d; _i++) {\n", field->array_size);
                sstr_printf_append(source, "            %S_init(&obj->%S[_i]);\n", field->type_name, field->name);
                sstr_append_cstr(source, "        }\n    }\n");
            } else {
                sstr_printf_append(source, "    memset(obj->%S, 0, sizeof(obj->%S));\n", field->name, field->name);
            }
            continue;
        }
        if (field->is_array) {
            // dynamic array: an extra integer field XXX_len denoting the size
            // of the array.
            sstr_printf_append(source, "    obj->%S = NULL;\n", field->name);
            sstr_printf_append(source, "    obj->%S_len = 0;\n", field->name);
            continue;
        }
        switch (field->type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S;\n", field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                }
                break;
            case FIELD_TYPE_LONG:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S;\n", field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                }
                break;
            case FIELD_TYPE_INT8:
            case FIELD_TYPE_INT16:
            case FIELD_TYPE_INT32:
            case FIELD_TYPE_INT64:
            case FIELD_TYPE_UINT8:
            case FIELD_TYPE_UINT16:
            case FIELD_TYPE_UINT32:
            case FIELD_TYPE_UINT64:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S;\n", field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                }
                break;
            case FIELD_TYPE_FLOAT:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S;\n", field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                }
                break;
            case FIELD_TYPE_DOUBLE:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S;\n", field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0.0;\n", field->name);
                }
                break;
            case FIELD_TYPE_SSTR:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = sstr(\"%S\");\n",
                                       field->name, field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = NULL;\n",
                                       field->name);
                }
                break;
            case FIELD_TYPE_STRUCT:
                sstr_printf_append(source, "    %S_init(&obj->%S);\n",
                                   field->type_name, field->name);
                break;
            case FIELD_TYPE_ENUM:
                if (field->has_default) {
                    sstr_printf_append(source, "    obj->%S = %S_%S;\n",
                                       field->name, field->type_name,
                                       field->default_value);
                } else {
                    sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
                }
                break;
        }
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

// clear function for a single struct
// Helper: emit code to clear a single map container accessed via `expr`
// (e.g. "obj->field" or "obj->field[_mj]").
// Uses loop var `_mi`. Caller ensures surrounding braces if needed.
static void gen_clear_map_entries(struct struct_field* field, const char* expr,
                                  sstr_t source) {
    sstr_printf_append(source,
        "        { int _mi;\n"
        "        for (_mi = 0; _mi < %s.len; _mi++) {\n"
        "            sstr_free(%s.entries[_mi].key);\n",
        expr, expr);
    if (field->map_value_type == FIELD_TYPE_SSTR) {
        sstr_printf_append(source,
            "            sstr_free(%s.entries[_mi].value);\n", expr);
    } else if (field->map_value_type == FIELD_TYPE_STRUCT) {
        sstr_printf_append(source,
            "            %S_clear(&%s.entries[_mi].value);\n",
            field->map_value_type_name, expr);
    }
    sstr_printf_append(source,
        "        }\n"
        "        JGENC_FREE(%s.entries);\n"
        "        %s.entries = NULL;\n"
        "        %s.len = 0; }\n",
        expr, expr, expr);
}

static void gen_code_struct_clear(struct struct_container* st, sstr_t source) {
    sstr_printf_append(source, "int %S_clear(struct %S*obj) {\n", st->name,
                       st->name);
    int have_i = 0;
    struct struct_field* field = st->fields;
    for (; field; field = field->next) {
        if (field->is_optional || field->is_nullable) {
            sstr_printf_append(source, "    obj->has_%S = false;\n", field->name);
        }
        if (field->type == FIELD_TYPE_MAP) {
            if (field->is_array && field->array_size == 0) {
                // array of maps: outer loop over map containers
                char expr[256];
                snprintf(expr, sizeof(expr), "obj->%s[_mj]", sstr_cstr(field->name));
                sstr_printf_append(source,
                    "    { int _mj;\n"
                    "    for (_mj = 0; _mj < obj->%S_len; _mj++) {\n",
                    field->name);
                gen_clear_map_entries(field, expr, source);
                sstr_printf_append(source,
                    "    }\n"
                    "    JGENC_FREE(obj->%S);\n"
                    "    obj->%S = NULL;\n"
                    "    obj->%S_len = 0; }\n",
                    field->name, field->name, field->name);
            } else {
                // scalar map: clear entries directly
                char expr[256];
                snprintf(expr, sizeof(expr), "obj->%s", sstr_cstr(field->name));
                gen_clear_map_entries(field, expr, source);
            }
            continue;
        }
        if (field->is_array && field->array_size > 0) {
            // fixed-size array: clear elements in-place, no free
            if (field->type == FIELD_TYPE_STRUCT ||
                field->type == FIELD_TYPE_SSTR) {
                if (!have_i) {
                    have_i = 1;
                    sstr_append_cstr(source, "    int i;\n");
                }
                sstr_printf_append(source,
                                   "    for (i = 0; i < %d; i++) {\n",
                                   field->array_size);
                if (field->type == FIELD_TYPE_STRUCT) {
                    sstr_printf_append(source,
                                       "       %S_clear(&obj->%S[i]);\n",
                                       field->type_name, field->name);
                } else {
                    sstr_printf_append(
                        source, "       sstr_free(obj->%S[i]);\n", field->name);
                    sstr_printf_append(
                        source, "       obj->%S[i] = NULL;\n", field->name);
                }
                sstr_append_cstr(source, "    }\n");
            } else {
                sstr_printf_append(source, "    memset(obj->%S, 0, sizeof(obj->%S));\n",
                                   field->name, field->name);
            }
            continue;
        }
        if (field->is_array) {
            // dynamic array: free array, and set XXX_len=0
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
            sstr_printf_append(source, "    JGENC_FREE(obj->%S);\n", field->name);
            sstr_printf_append(source, "    obj->%S = NULL;\n", field->name);
            sstr_printf_append(source, "    obj->%S_len = 0;\n", field->name);
            continue;
        }

        switch (field->type) {
            case FIELD_TYPE_INT:
            case FIELD_TYPE_BOOL:
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_INT8:
            case FIELD_TYPE_INT16:
            case FIELD_TYPE_INT32:
            case FIELD_TYPE_INT64:
            case FIELD_TYPE_UINT8:
            case FIELD_TYPE_UINT16:
            case FIELD_TYPE_UINT32:
            case FIELD_TYPE_UINT64:
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
            case FIELD_TYPE_ENUM:
                sstr_printf_append(source, "    obj->%S = 0;\n", field->name);
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
        if (field->type == FIELD_TYPE_MAP && field->is_array && field->array_size == 0) {
            // map arrays have an extra _len field
            (*count)++;
        } else if (field->is_array && field->array_size == 0) {
            // dynamic arrays have an extra _len field
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
                       "    {0, sizeof(struct %S), %d, \"\", \"\", \"%S\", 0"
                       ", NULL, 0, 0, 0, 0, 0, -1},\n",
                       st->name, FIELD_TYPE_STRUCT, st->name, st->name,
                       st->name);
    sstr_t empty_s = sstr_new();
    gen_hash_arr(st->name, empty_s, param);
    sstr_free(empty_s);

    while (field) {
        // field list
        if (field->type == FIELD_TYPE_MAP) {
            const char* sfx = map_suffix(field);
            const char* enum_strings_expr = "NULL";
            const char* enum_count_expr = "0";
            sstr_t es_buf = NULL;
            sstr_t ec_buf = NULL;
            if (field->map_value_type == FIELD_TYPE_ENUM) {
                es_buf = sstr_dup(field->map_value_type_name);
                sstr_append_cstr(es_buf, "_enum_strings");
                enum_strings_expr = sstr_cstr(es_buf);
                ec_buf = sstr_dup(field->map_value_type_name);
                sstr_append_cstr(ec_buf, "_enum_count");
                enum_count_expr = sstr_cstr(ec_buf);
            }
            if (field->is_array && field->array_size == 0) {
                // array of maps: pointer field
                sstr_printf_append(param->source,
                    "    {offsetof(struct %S, %S), sizeof(struct json_map_%s), "
                    "%d, \"%S\", \"%S\", \"%S\", 1, %s, %s, 0, %d, "
                    "sizeof(struct json_map_entry_%s)",
                    st->name, field->name, sfx,
                    FIELD_TYPE_MAP, field->map_value_type_name,
                    JSON_KEY(field), st->name,
                    enum_strings_expr, enum_count_expr,
                    field->map_value_type, sfx);
                if (field->is_optional || field->is_nullable) {
                    sstr_printf_append(param->source, ", %d, offsetof(struct %S, has_%S)},\n",
                                       field->is_nullable, st->name, field->name);
                } else {
                    sstr_append_cstr(param->source, ", 0, -1},\n");
                }
                gen_hash_arr(st->name, JSON_KEY(field), param);
                // _len field
                sstr_printf_append(param->source,
                    "    {offsetof(struct %S, %S_len), sizeof(int), "
                    "%d, \"int\", \"%S_len\", \"%S\", 0, NULL, 0, 0, 0, 0, 0, -1},\n",
                    st->name, field->name, FIELD_TYPE_INT,
                    JSON_KEY(field), st->name);
                sstr_t tmp = sstr_dup(JSON_KEY(field));
                sstr_append_cstr(tmp, "_len");
                gen_hash_arr(st->name, tmp, param);
                sstr_free(tmp);
            } else {
                // scalar map field
                sstr_printf_append(param->source,
                    "    {offsetof(struct %S, %S), sizeof(struct json_map_%s), "
                    "%d, \"%S\", \"%S\", \"%S\", 0, %s, %s, 0, %d, "
                    "sizeof(struct json_map_entry_%s)",
                    st->name, field->name, sfx,
                    FIELD_TYPE_MAP, field->map_value_type_name,
                    JSON_KEY(field), st->name,
                    enum_strings_expr, enum_count_expr,
                    field->map_value_type, sfx);
                if (field->is_optional || field->is_nullable) {
                    sstr_printf_append(param->source, ", %d, offsetof(struct %S, has_%S)},\n",
                                       field->is_nullable, st->name, field->name);
                } else {
                    sstr_append_cstr(param->source, ", 0, -1},\n");
                }
                gen_hash_arr(st->name, JSON_KEY(field), param);
            }
            if (es_buf) sstr_free(es_buf);
            if (ec_buf) sstr_free(ec_buf);
        } else if (field->type == FIELD_TYPE_STRUCT) {
            sstr_printf_append(param->source,
                               "    {offsetof(struct %S, %S), sizeof(struct "
                               "%S), %d, \"%S\", \"%S\", "
                               "\"%S\", %d, NULL, 0, %d, 0, 0",
                               st->name, field->name, field->type_name,
                               field->type, field->type_name, JSON_KEY(field),
                               st->name, field->is_array, field->array_size);
            if (field->is_optional || field->is_nullable) {
                sstr_printf_append(param->source, ", %d, offsetof(struct %S, has_%S)},\n",
                                   field->is_nullable, st->name, field->name);
            } else {
                sstr_append_cstr(param->source, ", 0, -1},\n");
            }
            gen_hash_arr(st->name, JSON_KEY(field), param);
        } else if (field->type == FIELD_TYPE_ENUM) {
            sstr_printf_append(
                param->source,
                "    {offsetof(struct %S, %S), sizeof(int), %d, \"%S\", \"%S\", "
                "\"%S\", %d, %S_enum_strings, %S_enum_count, %d, 0, 0",
                st->name, field->name, field->type,
                field->type_name, JSON_KEY(field),
                st->name, field->is_array,
                field->type_name, field->type_name, field->array_size);
            if (field->is_optional || field->is_nullable) {
                sstr_printf_append(param->source, ", %d, offsetof(struct %S, has_%S)},\n",
                                   field->is_nullable, st->name, field->name);
            } else {
                sstr_append_cstr(param->source, ", 0, -1},\n");
            }
            gen_hash_arr(st->name, JSON_KEY(field), param);
        } else {
            sstr_printf_append(
                param->source,
                "    {offsetof(struct %S, %S), sizeof(%S), %d, \"%S\", \"%S\", "
                "\"%S\", %d, NULL, 0, %d, 0, 0",
                st->name, field->name, field->type_name, field->type,
                field->type_name, JSON_KEY(field), st->name, field->is_array,
                field->array_size);
            if (field->is_optional || field->is_nullable) {
                sstr_printf_append(param->source, ", %d, offsetof(struct %S, has_%S)},\n",
                                   field->is_nullable, st->name, field->name);
            } else {
                sstr_append_cstr(param->source, ", 0, -1},\n");
            }
            gen_hash_arr(st->name, JSON_KEY(field), param);
        }
        if (field->type != FIELD_TYPE_MAP && field->is_array && field->array_size == 0) {
            // dynamic array: generate _len field entry
            sstr_printf_append(param->source, "    {offsetof(struct %S, %S_len), sizeof(int), "
                               "%d, \"int\", \"%S_len\", "
                               "\"%S\", %d, NULL, 0, 0, 0, 0, 0, -1},\n",
                               st->name, field->name, FIELD_TYPE_INT,
                               JSON_KEY(field), st->name, 0);
            sstr_t tmp = sstr_dup(JSON_KEY(field));
            sstr_append_cstr(tmp, "_len");
            gen_hash_arr(st->name, tmp, param);
            sstr_free(tmp);
        }

        field = field->next;
    }
}

// generate enum type definition and constants for each enum in the header
static void gen_enum_header_fn(void* key, void* value, void* ptr) {
    (void)key;
    struct enum_container* ec = (struct enum_container*)value;
    sstr_t header = (sstr_t)ptr;

    // Generate C enum type
    sstr_printf_append(header, "enum %S {\n", ec->name);
    struct enum_value* v = ec->values;
    while (v) {
        sstr_printf_append(header, "    %S_%S = %d", ec->name, v->name, v->index);
        if (v->next) {
            sstr_append_cstr(header, ",");
        }
        sstr_append_cstr(header, "\n");
        v = v->next;
    }
    sstr_append_cstr(header, "};\n\n");
}

static void gen_enum_headers(struct hash_map* enum_map, sstr_t header) {
    hash_map_for_each(enum_map, gen_enum_header_fn, header);
}

// generate enum string arrays for each enum
static void gen_enum_string_fn(void* key, void* value, void* ptr) {
    (void)key;
    struct enum_container* ec = (struct enum_container*)value;
    sstr_t source = (sstr_t)ptr;

    sstr_printf_append(source, "static const char* %S_enum_strings[] = {",
                       ec->name);
    struct enum_value* v = ec->values;
    int first = 1;
    while (v) {
        if (!first) {
            sstr_append_cstr(source, ", ");
        }
        sstr_printf_append(source, "\"%S\"", v->name);
        first = 0;
        v = v->next;
    }
    sstr_append_cstr(source, "};\n");
    sstr_printf_append(source, "static const int %S_enum_count = %d;\n\n",
                       ec->name, ec->count);
}

static void gen_enum_strings(struct hash_map* enum_map, sstr_t source) {
    hash_map_for_each(enum_map, gen_enum_string_fn, source);
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
                     "struct json_field_offset_item {\n"
                     "    int offset;\n"
                     "    int type_size;\n"
                     "    int field_type;\n"
                     "    const char* field_type_name;\n"
                     "    const char* field_name;\n"
                     "    const char* struct_name;\n"
                     "    int is_array;\n"
                     "    const char** enum_strings;\n"
                     "    int enum_count;\n"
                     "    int array_size;\n"
                     "    int map_value_type;\n"
                     "    int map_entry_size;\n"
                     "    int is_nullable;\n"
                     "    int has_field_offset;\n"
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
    sstr_append_cstr(source, "    {0, 0, 0, NULL, NULL, NULL, 0, NULL, 0, 0, 0, 0, 0, -1}};\n");

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
        if (iter->type == FIELD_TYPE_MAP && iter->map_value_type == FIELD_TYPE_STRUCT) {
            int r = hash_map_find(dep_map, iter->map_value_type_name, &dv);
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
    sstr_append_cstr(head, "#include <stdbool.h>\n");
    sstr_append_cstr(head, "#include <stdint.h>\n");
    sstr_append_cstr(head, "#ifdef __cplusplus\n");
    sstr_append_cstr(head, "extern \"C\" {\n");
    sstr_append_cstr(head, "#endif\n\n");
    sstr_append_cstr(
        head,
        "#ifndef JGENC_MALLOC\n"
        "/**\n"
        " * @brief Override the memory allocator used by generated JSON code.\n"
        " *\n"
        " * Pass NULL for any parameter to keep the default (stdlib).\n"
        " * Not thread-safe — call once during program initialisation.\n"
        " */\n"
        "void json_gen_c_set_alloc(void* (*malloc_fn)(size_t),\n"
        "                          void* (*realloc_fn)(void*, size_t),\n"
        "                          void  (*free_fn)(void*));\n"
        "#endif\n\n");
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
        "out);\n"
        "int json_marshal_array_indent_int8_t(int8_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_int16_t(int16_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_int32_t(int32_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_int64_t(int64_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_uint8_t(uint8_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_uint16_t(uint16_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_uint32_t(uint32_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n"
        "int json_marshal_array_indent_uint64_t(uint64_t* obj, int len, int "
        "indent, int curindent, sstr_t out);\n\n"
        "#define json_marshal_array_int(obj, len, out) "
        "json_marshal_array_indent_int(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_long(obj, len, out) "
        "json_marshal_array_indent_long(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_float(obj, len, out) "
        "json_marshal_array_indent_float(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_double(obj, len, out) "
        "json_marshal_array_indent_double(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_sstr_t(obj, len, out) "
        "json_marshal_array_indent_sstr_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_int8_t(obj, len, out) "
        "json_marshal_array_indent_int8_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_int16_t(obj, len, out) "
        "json_marshal_array_indent_int16_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_int32_t(obj, len, out) "
        "json_marshal_array_indent_int32_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_int64_t(obj, len, out) "
        "json_marshal_array_indent_int64_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_uint8_t(obj, len, out) "
        "json_marshal_array_indent_uint8_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_uint16_t(obj, len, out) "
        "json_marshal_array_indent_uint16_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_uint32_t(obj, len, out) "
        "json_marshal_array_indent_uint32_t(obj, len, 0, 0, out)\n"
        "#define json_marshal_array_uint64_t(obj, len, out) "
        "json_marshal_array_indent_uint64_t(obj, len, 0, 0, out)\n\n"

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
        "len);\n"
        "int json_unmarshal_array_int8_t(sstr_t content, int8_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_int16_t(sstr_t content, int16_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_int32_t(sstr_t content, int32_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_int64_t(sstr_t content, int64_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_uint8_t(sstr_t content, uint8_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_uint16_t(sstr_t content, uint16_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_uint32_t(sstr_t content, uint32_t** ptr, "
        "int* len);\n"
        "int json_unmarshal_array_uint64_t(sstr_t content, uint64_t** ptr, "
        "int* len);\n\n");

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
                       "#include <malloc.h>\n#include <string.h>\n\n",
                       OUTPUT_H_FILENAME);

    /* Emit allocator indirection macros early, so both gencode-emitted
       struct code and the json_parse.c template can use JGENC_*. */
    sstr_append_cstr(source,
        "#ifndef JGENC_MALLOC\n"
        "static void* (*jgenc_malloc_fn_)(size_t)                        = NULL;\n"
        "static void* (*jgenc_realloc_fn_)(void*, size_t)                = NULL;\n"
        "static void  (*jgenc_free_fn_)(void*)                           = NULL;\n"
        "\n"
        "static inline void* jgenc_malloc_dispatch_(size_t sz) {\n"
        "    return jgenc_malloc_fn_ ? jgenc_malloc_fn_(sz) : malloc(sz);\n"
        "}\n"
        "static inline void* jgenc_realloc_dispatch_(void* p, size_t sz) {\n"
        "    return jgenc_realloc_fn_ ? jgenc_realloc_fn_(p, sz) : realloc(p, sz);\n"
        "}\n"
        "static inline void jgenc_free_dispatch_(void* p) {\n"
        "    if (jgenc_free_fn_) jgenc_free_fn_(p); else free(p);\n"
        "}\n"
        "\n"
        "#define JGENC_MALLOC(sz)      jgenc_malloc_dispatch_(sz)\n"
        "#define JGENC_REALLOC(p, sz)  jgenc_realloc_dispatch_((p), (sz))\n"
        "#define JGENC_FREE(p)         jgenc_free_dispatch_(p)\n"
        "\n"
        "void json_gen_c_set_alloc(void* (*malloc_fn)(size_t),\n"
        "                           void* (*realloc_fn)(void*, size_t),\n"
        "                           void  (*free_fn)(void*)) {\n"
        "    jgenc_malloc_fn_  = malloc_fn;\n"
        "    jgenc_realloc_fn_ = realloc_fn;\n"
        "    jgenc_free_fn_    = free_fn;\n"
        "}\n"
        "#else\n"
        "/* Custom compile-time macros supplied */\n"
        "#endif\n"
        "#ifndef JGENC_REALLOC\n"
        "#define JGENC_REALLOC(p, sz) realloc((p), (sz))\n"
        "#endif\n"
        "#ifndef JGENC_FREE\n"
        "#define JGENC_FREE(p) free(p)\n"
        "#endif\n\n");

    sstr_append_of(source, json_parse_h, (size_t)json_parse_h_len);
    gen_code_scalar_marshal_array(source);
    return 0;
}

int gencode_source_end(sstr_t source) {
    sstr_append_of(source, json_parse_c, (size_t)json_parse_c_len);
    return 0;
}

int gencode_source(struct hash_map* struct_map, struct hash_map* enum_map, sstr_t source, sstr_t header) {
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

    // generate enum type definitions and constants in header
    gen_enum_headers(enum_map, header);

    // includes, and all common functions, scalar type parsing codes.
    gencode_source_begin(source);

    // generate enum string arrays (must be before offset map which references them)
    gen_enum_strings(enum_map, source);

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
