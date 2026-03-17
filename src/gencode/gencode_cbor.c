/**
 * @file gencode_cbor.c
 * @brief Generate CBOR pack/unpack C code from parsed schema.
 *
 * Parallel to gencode_msgpack.c but emits CBOR (RFC 8949) codec
 * using the cbor_codec.h/c embedded runtime.
 */

#include "gencode/gencode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"
#include "utils/hash.h"

#define DEPENDENCY_HASH_MAP_BUCKET_SIZE 4096

/* The effective wire key for a field: @json alias if set, else C name. */
#define WIRE_KEY(f) sstr_cstr((f)->json_name ? (f)->json_name : (f)->name)

/* ── helpers ─────────────────────────────────────────────────────────── */

static void cb_map_c_value_type(struct struct_field *field, sstr_t out) {
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

static const char *cb_map_suffix(struct struct_field *field) {
    if (field->map_value_type == FIELD_TYPE_ENUM ||
        field->map_value_type == FIELD_TYPE_BOOL) {
        return "int";
    }
    return sstr_cstr(field->map_value_type_name);
}

static int mp_count_struct_fields(struct struct_container *st) {
    int count = 0;
    struct struct_field *f = st->fields;
    while (f) { count++; f = f->next; }
    return count;
}

inline static unsigned int mp_hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int h = 0xbc9f1d34;
    h = hash_murmur(sstr_cstr(key1), sstr_length(key1), h);
    h = hash_murmur("#", 1, h);
    h = hash_murmur(sstr_cstr(key2), sstr_length(key2), h);
    return h;
}

/* ── header generation ──────────────────────────────────────────────── */

/* Same struct definitions as JSON (the types don't change). */
static void cb_gen_struct_header(struct struct_container *st, sstr_t header) {
    /* Map container/entry types (idempotent guards) */
    struct struct_field *field = st->fields;
    while (field) {
        if (field->type == FIELD_TYPE_MAP) {
            sstr_t suffix = sstr_new();
            cb_map_c_value_type(field, suffix);
            sstr_printf_append(
                header,
                "#ifndef _MAP_ENTRY_%s_DEFINED_\n"
                "#define _MAP_ENTRY_%s_DEFINED_\n"
                "struct map_entry_%s {\n"
                "    sstr_t key;\n"
                "    %s value;\n"
                "};\n"
                "struct map_container_%s {\n"
                "    struct map_entry_%s *entries;\n"
                "    int len;\n"
                "};\n"
                "#endif\n\n",
                cb_map_suffix(field), cb_map_suffix(field),
                cb_map_suffix(field), sstr_cstr(suffix),
                cb_map_suffix(field), cb_map_suffix(field));
            sstr_free(suffix);
        }
        field = field->next;
    }

    /* struct definition */
    sstr_printf_append(header, "struct %s {\n", sstr_cstr(st->name));
    field = st->fields;
    while (field) {
        /* optional/nullable → has_<field> bool */
        if (field->is_optional || field->is_nullable) {
            sstr_printf_append(header, "    bool has_%s;\n",
                               sstr_cstr(field->name));
        }
        /* deprecation attribute */
        if (field->is_deprecated) {
            sstr_append_cstr(header,
                "    JGENC_DEPRECATED(\"field is deprecated\") ");
        } else {
            sstr_append_cstr(header, "    ");
        }
        /* type */
        const char *ptr_suffix = (field->is_array && field->array_size == 0) ? "*" : "";
        if (field->type == FIELD_TYPE_MAP) {
            sstr_printf_append(header, "struct map_container_%s %s%s",
                               cb_map_suffix(field), ptr_suffix, sstr_cstr(field->name));
        } else if (field->type == FIELD_TYPE_STRUCT) {
            sstr_printf_append(header, "struct %s %s%s",
                               sstr_cstr(field->type_name),
                               ptr_suffix, sstr_cstr(field->name));
        } else if (field->type == FIELD_TYPE_ONEOF) {
            sstr_printf_append(header, "struct %s %s%s",
                               sstr_cstr(field->type_name),
                               ptr_suffix, sstr_cstr(field->name));
        } else if (field->type == FIELD_TYPE_ENUM) {
            sstr_printf_append(header, "int %s%s", ptr_suffix, sstr_cstr(field->name));
        } else if (field->type == FIELD_TYPE_BOOL) {
            sstr_printf_append(header, "int %s%s", ptr_suffix, sstr_cstr(field->name));
        } else {
            sstr_printf_append(header, "%s %s%s",
                               sstr_cstr(field->type_name),
                               ptr_suffix, sstr_cstr(field->name));
        }
        /* array suffix */
        if (field->is_array && field->array_size > 0) {
            sstr_printf_append(header, "[%d]", field->array_size);
        } else if (field->is_array && field->array_size == 0) {
            /* dynamic array pointer: already have "type name", nothing to add;
               pointer was inserted by the type emission logic below. */
        }
        sstr_append_cstr(header, ";\n");

        /* dynamic array length companion */
        if (field->is_array && field->array_size == 0) {
            sstr_printf_append(header, "    int %s_len;\n",
                               sstr_cstr(field->name));
        }
        field = field->next;
    }
    sstr_append_cstr(header, "};\n\n");

    /* init / clear / pack / unpack declarations */
    sstr_printf_append(
        header,
        "int %s_init(struct %s *obj);\n"
        "int %s_clear(struct %s *obj);\n"
        "int cbor_pack_%s(struct %s *obj, sstr_t out);\n"
        "int cbor_unpack_%s(const unsigned char *data, size_t len, struct %s *obj);\n"
        "int cbor_pack_array_%s(struct %s *obj, int count, sstr_t out);\n"
        "int cbor_unpack_array_%s(const unsigned char *data, size_t len, struct %s **obj, int *count);\n\n",
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name));

    /* field index enum + mask word count */
    int field_count = mp_count_struct_fields(st);
    sstr_printf_append(header, "enum %s_field_index {\n",
                       sstr_cstr(st->name));
    field = st->fields;
    int idx = 0;
    while (field) {
        sstr_printf_append(header, "    %s_FIELD_%s = %d,\n",
                           sstr_cstr(st->name), sstr_cstr(field->name), idx);
        idx++;
        field = field->next;
    }
    sstr_printf_append(header, "    %s_FIELD_COUNT = %d\n};\n",
                       sstr_cstr(st->name), field_count);
    int mask_words = (field_count <= 0) ? 1 : ((field_count + 63) / 64);
    sstr_printf_append(header, "#define %s_FIELD_MASK_WORD_COUNT %d\n\n",
                       sstr_cstr(st->name), mask_words);
}

/* ── enum headers (same as JSON) ────────────────────────────────────── */

static void cb_gen_enum_header_fn(void *key, void *value, void *ptr) {
    (void)key;
    struct enum_container *ec = (struct enum_container *)value;
    sstr_t header = (sstr_t)ptr;

    sstr_printf_append(header, "enum %s {\n", sstr_cstr(ec->name));
    struct enum_value *ev = ec->values;
    int i = 0;
    while (ev) {
        if (ev->is_deprecated) {
            sstr_printf_append(header, "    %s_%s = %d JGENC_DEPRECATED_ENUM(\"deprecated\"),\n",
                               sstr_cstr(ec->name), sstr_cstr(ev->name), i);
        } else {
            sstr_printf_append(header, "    %s_%s = %d,\n",
                               sstr_cstr(ec->name), sstr_cstr(ev->name), i);
        }
        i++;
        ev = ev->next;
    }
    sstr_printf_append(header, "    %s_enum_count = %d\n};\n\n",
                       sstr_cstr(ec->name), i);
}

/* ── enum string arrays ─────────────────────────────────────────────── */

static void cb_gen_enum_string_fn(void *key, void *value, void *ptr) {
    (void)key;
    struct enum_container *ec = (struct enum_container *)value;
    sstr_t source = (sstr_t)ptr;

    sstr_printf_append(source, "static const char *%s_enum_strings[] = {",
                       sstr_cstr(ec->name));
    struct enum_value *ev = ec->values;
    int first = 1;
    while (ev) {
        sstr_printf_append(source, "%s\"%s\"", first ? "" : ", ",
                           sstr_cstr(ev->name));
        first = 0;
        ev = ev->next;
    }
    sstr_append_cstr(source, "};\n");
    int count = 0;
    ev = ec->values;
    while (ev) { count++; ev = ev->next; }
    sstr_printf_append(source, "static const int %s_enum_str_count = %d;\n\n",
                       sstr_cstr(ec->name), count);
}

/* ── oneof header ───────────────────────────────────────────────────── */

static void cb_gen_oneof_header(struct oneof_container *oc, sstr_t header) {
    /* tag enum */
    sstr_printf_append(header, "enum %s_tag {\n", sstr_cstr(oc->name));
    struct oneof_variant *var = oc->variants;
    int i = 0;
    while (var) {
        if (var->is_deprecated) {
            sstr_printf_append(header, "    %s_%s = %d JGENC_DEPRECATED_ENUM(\"deprecated\"),\n",
                               sstr_cstr(oc->name), sstr_cstr(var->name), i);
        } else {
            sstr_printf_append(header, "    %s_%s = %d,\n",
                               sstr_cstr(oc->name), sstr_cstr(var->name), i);
        }
        i++;
        var = var->next;
    }
    sstr_printf_append(header, "    %s_tag_count = %d\n};\n\n",
                       sstr_cstr(oc->name), i);

    /* oneof struct with union */
    sstr_printf_append(header, "struct %s {\n    enum %s_tag tag;\n    union {\n",
                       sstr_cstr(oc->name), sstr_cstr(oc->name));
    var = oc->variants;
    while (var) {
        if (var->is_deprecated) {
            sstr_printf_append(header,
                "        JGENC_DEPRECATED(\"variant is deprecated\") struct %s %s;\n",
                sstr_cstr(var->struct_type_name), sstr_cstr(var->name));
        } else {
            sstr_printf_append(header, "        struct %s %s;\n",
                               sstr_cstr(var->struct_type_name), sstr_cstr(var->name));
        }
        var = var->next;
    }
    sstr_append_cstr(header, "    } value;\n};\n\n");

    /* function declarations */
    sstr_printf_append(
        header,
        "int %s_init(struct %s *obj);\n"
        "int %s_clear(struct %s *obj);\n"
        "int cbor_pack_%s(struct %s *obj, sstr_t out);\n"
        "int cbor_unpack_%s(const unsigned char *data, size_t len, struct %s *obj);\n"
        "int cbor_pack_array_%s(struct %s *obj, int count, sstr_t out);\n"
        "int cbor_unpack_array_%s(const unsigned char *data, size_t len, struct %s **obj, int *count);\n\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name));
}

/* ── oneof tag strings ──────────────────────────────────────────────── */

static void cb_gen_oneof_tag_strings(struct oneof_container *oc, sstr_t source) {
    sstr_printf_append(source, "static const char *%s_tag_strings[] = {",
                       sstr_cstr(oc->name));
    struct oneof_variant *var = oc->variants;
    int first = 1;
    while (var) {
        sstr_printf_append(source, "%s\"%s\"", first ? "" : ", ",
                           sstr_cstr(var->name));
        first = 0;
        var = var->next;
    }
    sstr_append_cstr(source, "};\n\n");
}

static void cb_gen_oneof_statics_fn(void *key, void *value, void *ptr) {
    (void)key;
    struct oneof_container *oc = (struct oneof_container *)value;
    sstr_t source = (sstr_t)ptr;
    cb_gen_oneof_tag_strings(oc, source);
}

/* ── init / clear (shared logic, same as JSON) ──────────────────────── */

static void cb_gen_struct_init(struct struct_container *st, sstr_t source) {
    sstr_printf_append(source,
        "int %s_init(struct %s *obj) {\n"
        "    memset(obj, 0, sizeof(struct %s));\n",
        sstr_cstr(st->name), sstr_cstr(st->name), sstr_cstr(st->name));

    struct struct_field *f = st->fields;
    while (f) {
        if (f->type == FIELD_TYPE_SSTR && !f->is_array) {
            sstr_printf_append(source, "    obj->%s = sstr_new();\n",
                               sstr_cstr(f->name));
        }
        if (f->default_value) {
            if (f->type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source, "    sstr_append_cstr(obj->%s, \"%s\");\n",
                                   sstr_cstr(f->name), sstr_cstr(f->default_value));
            } else if (f->type == FIELD_TYPE_ENUM) {
                sstr_printf_append(source, "    obj->%s = %s_%s;\n",
                                   sstr_cstr(f->name),
                                   sstr_cstr(f->type_name),
                                   sstr_cstr(f->default_value));
            } else {
                sstr_printf_append(source, "    obj->%s = %s;\n",
                                   sstr_cstr(f->name),
                                   sstr_cstr(f->default_value));
            }
        }
        /* Nested struct init */
        if (f->type == FIELD_TYPE_STRUCT && !f->is_array) {
            sstr_printf_append(source, "    %s_init(&obj->%s);\n",
                               sstr_cstr(f->type_name), sstr_cstr(f->name));
        }
        if (f->type == FIELD_TYPE_ONEOF && !f->is_array) {
            sstr_printf_append(source, "    %s_init(&obj->%s);\n",
                               sstr_cstr(f->type_name), sstr_cstr(f->name));
        }
        f = f->next;
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

static void cb_gen_clear_map_entries(struct struct_field *field,
                                     const char *expr, sstr_t source) {
    sstr_printf_append(source,
        "    for (int _mi = 0; _mi < %s.len; _mi++) {\n"
        "        sstr_free(%s.entries[_mi].key);\n",
        expr, expr);
    if (field->map_value_type == FIELD_TYPE_SSTR) {
        sstr_printf_append(source,
            "        sstr_free(%s.entries[_mi].value);\n", expr);
    } else if (field->map_value_type == FIELD_TYPE_STRUCT) {
        sstr_printf_append(source,
            "        %s_clear(&%s.entries[_mi].value);\n",
            sstr_cstr(field->map_value_type_name), expr);
    }
    sstr_printf_append(source,
        "    }\n"
        "    JGENC_FREE(%s.entries);\n"
        "    %s.entries = NULL;\n"
        "    %s.len = 0;\n",
        expr, expr, expr);
}

static void cb_gen_struct_clear(struct struct_container *st, sstr_t source) {
    sstr_printf_append(source,
        "int %s_clear(struct %s *obj) {\n",
        sstr_cstr(st->name), sstr_cstr(st->name));

    /* Check if clear body will reference obj at all. */
    int has_cleanup = 0;
    struct struct_field *f = st->fields;
    while (f) {
        if (f->type == FIELD_TYPE_SSTR || f->type == FIELD_TYPE_STRUCT ||
            f->type == FIELD_TYPE_ONEOF || f->type == FIELD_TYPE_MAP ||
            f->is_array) {
            has_cleanup = 1;
            break;
        }
        f = f->next;
    }
    if (!has_cleanup) {
        sstr_append_cstr(source, "    (void)obj;\n");
    }

    f = st->fields;
    while (f) {
        char base[256];
        snprintf(base, sizeof(base), "obj->%s", sstr_cstr(f->name));

        if (f->type == FIELD_TYPE_SSTR && !f->is_array) {
            sstr_printf_append(source, "    sstr_free(%s);\n", base);
        } else if (f->type == FIELD_TYPE_STRUCT && !f->is_array) {
            sstr_printf_append(source, "    %s_clear(&%s);\n",
                               sstr_cstr(f->type_name), base);
        } else if (f->type == FIELD_TYPE_ONEOF && !f->is_array) {
            sstr_printf_append(source, "    %s_clear(&%s);\n",
                               sstr_cstr(f->type_name), base);
        } else if (f->type == FIELD_TYPE_MAP && !f->is_array) {
            cb_gen_clear_map_entries(f, base, source);
        } else if (f->is_array && f->array_size == 0) {
            /* dynamic array */
            if (f->type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < obj->%s_len; _i++) {\n"
                    "        sstr_free(%s[_i]);\n"
                    "    }\n", sstr_cstr(f->name), base);
            } else if (f->type == FIELD_TYPE_STRUCT) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < obj->%s_len; _i++) {\n"
                    "        %s_clear(&%s[_i]);\n"
                    "    }\n", sstr_cstr(f->name),
                    sstr_cstr(f->type_name), base);
            } else if (f->type == FIELD_TYPE_ONEOF) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < obj->%s_len; _i++) {\n"
                    "        %s_clear(&%s[_i]);\n"
                    "    }\n", sstr_cstr(f->name),
                    sstr_cstr(f->type_name), base);
            } else if (f->type == FIELD_TYPE_MAP) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < obj->%s_len; _i++) {\n",
                    sstr_cstr(f->name));
                char idx_expr[300];
                snprintf(idx_expr, sizeof(idx_expr), "%s[_i]", base);
                cb_gen_clear_map_entries(f, idx_expr, source);
                sstr_append_cstr(source, "    }\n");
            }
            sstr_printf_append(source,
                "    JGENC_FREE(%s);\n"
                "    %s = NULL;\n"
                "    obj->%s_len = 0;\n",
                base, base, sstr_cstr(f->name));
        } else if (f->is_array && f->array_size > 0) {
            /* fixed array */
            if (f->type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < %d; _i++) {\n"
                    "        sstr_free(%s[_i]);\n"
                    "    }\n", f->array_size, base);
            } else if (f->type == FIELD_TYPE_STRUCT) {
                sstr_printf_append(source,
                    "    for (int _i = 0; _i < %d; _i++) {\n"
                    "        %s_clear(&%s[_i]);\n"
                    "    }\n", f->array_size,
                    sstr_cstr(f->type_name), base);
            }
        }
        f = f->next;
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

/* ── oneof init / clear ─────────────────────────────────────────────── */

static void cb_gen_oneof_init(struct oneof_container *oc, sstr_t source) {
    sstr_printf_append(source,
        "int %s_init(struct %s *obj) {\n"
        "    memset(obj, 0, sizeof(struct %s));\n"
        "    return 0;\n}\n\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name), sstr_cstr(oc->name));
}

static void cb_gen_oneof_clear(struct oneof_container *oc, sstr_t source) {
    sstr_printf_append(source,
        "int %s_clear(struct %s *obj) {\n"
        "    switch (obj->tag) {\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name));
    struct oneof_variant *var = oc->variants;
    while (var) {
        sstr_printf_append(source,
            "    case %s_%s:\n"
            "        %s_clear(&obj->value.%s);\n"
            "        break;\n",
            sstr_cstr(oc->name), sstr_cstr(var->name),
            sstr_cstr(var->struct_type_name), sstr_cstr(var->name));
        var = var->next;
    }
    sstr_append_cstr(source,
        "    default: break;\n"
        "    }\n"
        "    return 0;\n}\n\n");
}

/* ── pack (marshal) ─────────────────────────────────────────────────── */

/* Pack a single scalar field value. */
static void cb_gen_pack_scalar(struct struct_field *f, const char *accessor,
                               sstr_t source) {
    switch (f->type) {
    case FIELD_TYPE_INT:
    case FIELD_TYPE_INT8:
    case FIELD_TYPE_INT16:
    case FIELD_TYPE_INT32:
        sstr_printf_append(source, "    cb_pack_int(out, (int64_t)%s);\n", accessor);
        break;
    case FIELD_TYPE_LONG:
    case FIELD_TYPE_INT64:
        sstr_printf_append(source, "    cb_pack_int(out, (int64_t)%s);\n", accessor);
        break;
    case FIELD_TYPE_UINT8:
    case FIELD_TYPE_UINT16:
    case FIELD_TYPE_UINT32:
        sstr_printf_append(source, "    cb_pack_uint(out, (uint64_t)%s);\n", accessor);
        break;
    case FIELD_TYPE_UINT64:
        sstr_printf_append(source, "    cb_pack_uint(out, (uint64_t)%s);\n", accessor);
        break;
    case FIELD_TYPE_FLOAT:
        sstr_printf_append(source, "    cb_pack_float(out, %s);\n", accessor);
        break;
    case FIELD_TYPE_DOUBLE:
        sstr_printf_append(source, "    cb_pack_double(out, %s);\n", accessor);
        break;
    case FIELD_TYPE_BOOL:
        sstr_printf_append(source, "    cb_pack_bool(out, %s);\n", accessor);
        break;
    case FIELD_TYPE_SSTR:
        sstr_printf_append(source, "    cb_pack_sstr(out, %s);\n", accessor);
        break;
    case FIELD_TYPE_ENUM:
        sstr_printf_append(source,
            "    if (%s >= 0 && %s < %s_enum_str_count) {\n"
            "        const char *_es = %s_enum_strings[%s];\n"
            "        cb_pack_str(out, _es, (uint32_t)strlen(_es));\n"
            "    } else {\n"
            "        cb_pack_int(out, (int64_t)%s);\n"
            "    }\n",
            accessor, accessor, sstr_cstr(f->type_name),
            sstr_cstr(f->type_name), accessor, accessor);
        break;
    case FIELD_TYPE_STRUCT:
        sstr_printf_append(source, "    cbor_pack_%s(&%s, out);\n",
                           sstr_cstr(f->type_name), accessor);
        break;
    case FIELD_TYPE_ONEOF:
        sstr_printf_append(source, "    cbor_pack_%s(&%s, out);\n",
                           sstr_cstr(f->type_name), accessor);
        break;
    default:
        break;
    }
}

/* Pack map value */
static void cb_gen_pack_map_value(struct struct_field *field,
                                  const char *val_access, sstr_t source) {
    switch (field->map_value_type) {
    case FIELD_TYPE_INT:
    case FIELD_TYPE_INT8: case FIELD_TYPE_INT16: case FIELD_TYPE_INT32:
    case FIELD_TYPE_LONG: case FIELD_TYPE_INT64:
        sstr_printf_append(source, "        cb_pack_int(out, (int64_t)%s);\n", val_access);
        break;
    case FIELD_TYPE_UINT8: case FIELD_TYPE_UINT16: case FIELD_TYPE_UINT32:
    case FIELD_TYPE_UINT64:
        sstr_printf_append(source, "        cb_pack_uint(out, (uint64_t)%s);\n", val_access);
        break;
    case FIELD_TYPE_FLOAT:
        sstr_printf_append(source, "        cb_pack_float(out, %s);\n", val_access);
        break;
    case FIELD_TYPE_DOUBLE:
        sstr_printf_append(source, "        cb_pack_double(out, %s);\n", val_access);
        break;
    case FIELD_TYPE_BOOL:
        sstr_printf_append(source, "        cb_pack_bool(out, %s);\n", val_access);
        break;
    case FIELD_TYPE_SSTR:
        sstr_printf_append(source, "        cb_pack_sstr(out, %s);\n", val_access);
        break;
    case FIELD_TYPE_STRUCT:
        sstr_printf_append(source, "        cbor_pack_%s(&%s, out);\n",
                           sstr_cstr(field->map_value_type_name), val_access);
        break;
    case FIELD_TYPE_ENUM:
        sstr_printf_append(source,
            "        if (%s >= 0 && %s < %s_enum_str_count) {\n"
            "            const char *_es = %s_enum_strings[%s];\n"
            "            cb_pack_str(out, _es, (uint32_t)strlen(_es));\n"
            "        } else {\n"
            "            cb_pack_int(out, (int64_t)%s);\n"
            "        }\n",
            val_access, val_access, sstr_cstr(field->map_value_type_name),
            sstr_cstr(field->map_value_type_name), val_access, val_access);
        break;
    default:
        break;
    }
}

static void cb_gen_pack_struct(struct struct_container *st, sstr_t source) {
    /* Count fields to pack (skip optional fields dynamically). */
    sstr_printf_append(source,
        "int cbor_pack_%s(struct %s *obj, sstr_t out) {\n",
        sstr_cstr(st->name), sstr_cstr(st->name));

    /* We need to count the actual number of fields at runtime for optional. */
    int has_optional = 0;
    struct struct_field *f = st->fields;
    int total_fields = 0;
    while (f) { total_fields++; if (f->is_optional) has_optional = 1; f = f->next; }

    if (has_optional) {
        sstr_printf_append(source, "    int _nfields = %d;\n", total_fields);
        f = st->fields;
        while (f) {
            if (f->is_optional) {
                sstr_printf_append(source,
                    "    if (!obj->has_%s) _nfields--;\n", sstr_cstr(f->name));
            }
            f = f->next;
        }
        sstr_append_cstr(source, "    cb_pack_map_header(out, (uint32_t)_nfields);\n");
    } else {
        sstr_printf_append(source,
            "    cb_pack_map_header(out, %d);\n", total_fields);
    }

    /* Pack each field as key-value pair. */
    f = st->fields;
    while (f) {
        const char *wire_key = WIRE_KEY(f);
        int key_len = (int)strlen(wire_key);

        if (f->is_optional) {
            sstr_printf_append(source, "    if (obj->has_%s) {\n",
                               sstr_cstr(f->name));
        }

        /* Pack key */
        sstr_printf_append(source,
            "    cb_pack_str(out, \"%s\", %d);\n", wire_key, key_len);

        /* Pack value */
        if (f->is_nullable) {
            sstr_printf_append(source,
                "    if (!obj->has_%s) {\n"
                "        cb_pack_nil(out);\n"
                "    } else {\n",
                sstr_cstr(f->name));
        }

        char accessor[256];
        snprintf(accessor, sizeof(accessor), "obj->%s", sstr_cstr(f->name));

        if (f->type == FIELD_TYPE_MAP && !f->is_array) {
            /* map field */
            sstr_printf_append(source,
                "    cb_pack_map_header(out, (uint32_t)%s.len);\n"
                "    for (int _mk = 0; _mk < %s.len; _mk++) {\n"
                "        cb_pack_sstr(out, %s.entries[_mk].key);\n",
                accessor, accessor, accessor);
            char val_acc[300];
            snprintf(val_acc, sizeof(val_acc), "%s.entries[_mk].value", accessor);
            cb_gen_pack_map_value(f, val_acc, source);
            sstr_append_cstr(source, "    }\n");
        } else if (f->is_array) {
            int len_known = (f->array_size > 0);
            if (len_known) {
                sstr_printf_append(source,
                    "    cb_pack_array_header(out, %d);\n"
                    "    for (int _ai = 0; _ai < %d; _ai++) {\n",
                    f->array_size, f->array_size);
            } else {
                sstr_printf_append(source,
                    "    cb_pack_array_header(out, (uint32_t)obj->%s_len);\n"
                    "    for (int _ai = 0; _ai < obj->%s_len; _ai++) {\n",
                    sstr_cstr(f->name), sstr_cstr(f->name));
            }
            /* Pack array element */
            char elem_acc[300];
            snprintf(elem_acc, sizeof(elem_acc), "%s[_ai]", accessor);

            /* Create a temporary struct_field for the element type. */
            struct struct_field elem_f = *f;
            elem_f.is_array = 0;
            cb_gen_pack_scalar(&elem_f, elem_acc, source);
            sstr_append_cstr(source, "    }\n");
        } else {
            cb_gen_pack_scalar(f, accessor, source);
        }

        if (f->is_nullable) {
            sstr_append_cstr(source, "    }\n");
        }
        if (f->is_optional) {
            sstr_append_cstr(source, "    }\n");
        }
        f = f->next;
    }
    sstr_append_cstr(source, "    return 0;\n}\n\n");
}

/* ── unpack (unmarshal) ─────────────────────────────────────────────── */

static void cb_gen_unpack_scalar(struct struct_field *f, const char *accessor,
                                 sstr_t source) {
    switch (f->type) {
    case FIELD_TYPE_INT:
    case FIELD_TYPE_INT8:
    case FIELD_TYPE_INT16:
    case FIELD_TYPE_INT32:
        sstr_printf_append(source,
            "    { int64_t _v; if (cb_unpack_number_as_int64(&_r, &_v) < 0) return -1;\n"
            "      %s = (%s)_v; }\n",
            accessor, sstr_cstr(f->type_name));
        break;
    case FIELD_TYPE_LONG:
        sstr_printf_append(source,
            "    { int64_t _v; if (cb_unpack_number_as_int64(&_r, &_v) < 0) return -1;\n"
            "      %s = (long)_v; }\n", accessor);
        break;
    case FIELD_TYPE_INT64:
        sstr_printf_append(source,
            "    { int64_t _v; if (cb_unpack_int64(&_r, &_v) < 0) return -1;\n"
            "      %s = _v; }\n", accessor);
        break;
    case FIELD_TYPE_UINT8:
    case FIELD_TYPE_UINT16:
    case FIELD_TYPE_UINT32:
        sstr_printf_append(source,
            "    { uint64_t _v; if (cb_unpack_number_as_uint64(&_r, &_v) < 0) return -1;\n"
            "      %s = (%s)_v; }\n",
            accessor, sstr_cstr(f->type_name));
        break;
    case FIELD_TYPE_UINT64:
        sstr_printf_append(source,
            "    { uint64_t _v; if (cb_unpack_uint64(&_r, &_v) < 0) return -1;\n"
            "      %s = _v; }\n", accessor);
        break;
    case FIELD_TYPE_FLOAT:
        sstr_printf_append(source,
            "    { double _v; if (cb_unpack_number_as_double(&_r, &_v) < 0) return -1;\n"
            "      %s = (float)_v; }\n", accessor);
        break;
    case FIELD_TYPE_DOUBLE:
        sstr_printf_append(source,
            "    { double _v; if (cb_unpack_number_as_double(&_r, &_v) < 0) return -1;\n"
            "      %s = _v; }\n", accessor);
        break;
    case FIELD_TYPE_BOOL:
        sstr_printf_append(source,
            "    { int _v; if (cb_unpack_bool(&_r, &_v) < 0) return -1;\n"
            "      %s = _v; }\n", accessor);
        break;
    case FIELD_TYPE_SSTR:
        sstr_printf_append(source,
            "    { const char *_s; uint32_t _sl;\n"
            "      if (cb_unpack_str(&_r, &_s, &_sl) < 0) return -1;\n"
            "      sstr_clear(%s);\n"
            "      sstr_append_of(%s, _s, _sl); }\n",
            accessor, accessor);
        break;
    case FIELD_TYPE_ENUM:
        sstr_printf_append(source,
            "    { const char *_s; uint32_t _sl;\n"
            "      if (cb_peek(&_r) == CB_NULL) { cb_unpack_nil(&_r); %s = 0; }\n"
            "      else if (((unsigned char)_r.data[_r.pos] >> 5) == CB_MAJOR_UINT ||\n"
            "               ((unsigned char)_r.data[_r.pos] >> 5) == CB_MAJOR_NINT) {\n"
            "          int64_t _v; if (cb_unpack_number_as_int64(&_r, &_v) < 0) return -1;\n"
            "          %s = (int)_v;\n"
            "      } else if (cb_unpack_str(&_r, &_s, &_sl) == 0) {\n"
            "          int _found = 0;\n"
            "          for (int _ei = 0; _ei < %s_enum_str_count; _ei++) {\n"
            "              if (_sl == (uint32_t)strlen(%s_enum_strings[_ei]) &&\n"
            "                  memcmp(_s, %s_enum_strings[_ei], _sl) == 0) {\n"
            "                  %s = _ei; _found = 1; break;\n"
            "              }\n"
            "          }\n"
            "          if (!_found) return -1;\n"
            "      } else { return -1; }\n"
            "    }\n",
            accessor, accessor,
            sstr_cstr(f->type_name), sstr_cstr(f->type_name),
            sstr_cstr(f->type_name), accessor);
        break;
    case FIELD_TYPE_STRUCT:
        sstr_printf_append(source,
            "    { size_t _before = _r.pos;\n"
            "      if (cbor_unpack_%s(_r.data + _r.pos, _r.len - _r.pos, &%s) < 0) return -1;\n"
            "      /* advance reader past consumed bytes */\n"
            "      struct cb_reader _tmp; cb_reader_init(&_tmp, _r.data + _before, _r.len - _before);\n"
            "      if (cb_unpack_skip(&_tmp) < 0) return -1;\n"
            "      _r.pos = _before + _tmp.pos;\n"
            "    }\n",
            sstr_cstr(f->type_name), accessor);
        break;
    case FIELD_TYPE_ONEOF:
        sstr_printf_append(source,
            "    { size_t _before = _r.pos;\n"
            "      if (cbor_unpack_%s(_r.data + _r.pos, _r.len - _r.pos, &%s) < 0) return -1;\n"
            "      struct cb_reader _tmp; cb_reader_init(&_tmp, _r.data + _before, _r.len - _before);\n"
            "      if (cb_unpack_skip(&_tmp) < 0) return -1;\n"
            "      _r.pos = _before + _tmp.pos;\n"
            "    }\n",
            sstr_cstr(f->type_name), accessor);
        break;
    default:
        sstr_append_cstr(source, "    cb_unpack_skip(&_r);\n");
        break;
    }
}

static void cb_gen_unpack_map_value(struct struct_field *field,
                                    const char *val_access, sstr_t source) {
    struct struct_field tmp = *field;
    tmp.type = field->map_value_type;
    tmp.type_name = field->map_value_type_name;
    tmp.is_array = 0;
    switch (field->map_value_type) {
    case FIELD_TYPE_INT: case FIELD_TYPE_INT8: case FIELD_TYPE_INT16:
    case FIELD_TYPE_INT32: case FIELD_TYPE_LONG: case FIELD_TYPE_INT64:
    case FIELD_TYPE_UINT8: case FIELD_TYPE_UINT16: case FIELD_TYPE_UINT32:
    case FIELD_TYPE_UINT64: case FIELD_TYPE_FLOAT: case FIELD_TYPE_DOUBLE:
    case FIELD_TYPE_BOOL: case FIELD_TYPE_SSTR: case FIELD_TYPE_ENUM:
    case FIELD_TYPE_STRUCT:
        cb_gen_unpack_scalar(&tmp, val_access, source);
        break;
    default:
        sstr_append_cstr(source, "            cb_unpack_skip(&_r);\n");
        break;
    }
}

static void cb_gen_unpack_struct(struct struct_container *st, sstr_t source) {
    sstr_printf_append(source,
        "int cbor_unpack_%s(const unsigned char *data, size_t len, struct %s *obj) {\n"
        "    struct cb_reader _r;\n"
        "    cb_reader_init(&_r, data, len);\n"
        "    uint32_t _nfields;\n"
        "    if (cb_unpack_map_header(&_r, &_nfields) < 0) return -1;\n"
        "    for (uint32_t _fi = 0; _fi < _nfields; _fi++) {\n"
        "        const char *_key; uint32_t _klen;\n"
        "        if (cb_unpack_str(&_r, &_key, &_klen) < 0) return -1;\n",
        sstr_cstr(st->name), sstr_cstr(st->name));

    /* Field dispatch by key name. */
    struct struct_field *f = st->fields;
    int first = 1;
    while (f) {
        const char *wire_key = WIRE_KEY(f);
        sstr_printf_append(source,
            "        %sif (_klen == %d && memcmp(_key, \"%s\", %d) == 0) {\n",
            first ? "" : "} else ", (int)strlen(wire_key), wire_key,
            (int)strlen(wire_key));
        first = 0;

        /* Handle nullable */
        if (f->is_nullable) {
            sstr_printf_append(source,
                "            if (cb_peek(&_r) == CB_NULL) {\n"
                "                cb_unpack_nil(&_r);\n"
                "                obj->has_%s = 0;\n"
                "            } else {\n"
                "                obj->has_%s = 1;\n",
                sstr_cstr(f->name), sstr_cstr(f->name));
        }

        if (f->is_optional && !f->is_nullable) {
            sstr_printf_append(source, "            obj->has_%s = 1;\n",
                               sstr_cstr(f->name));
        }

        char accessor[256];
        snprintf(accessor, sizeof(accessor), "obj->%s", sstr_cstr(f->name));

        if (f->type == FIELD_TYPE_MAP && !f->is_array) {
            /* map field */
            sstr_printf_append(source,
                "            { uint32_t _mc;\n"
                "              if (cb_unpack_map_header(&_r, &_mc) < 0) return -1;\n"
                "              %s.entries = (struct map_entry_%s *)JGENC_MALLOC(sizeof(struct map_entry_%s) * (_mc > 0 ? _mc : 1));\n"
                "              %s.len = (int)_mc;\n"
                "              for (uint32_t _mi = 0; _mi < _mc; _mi++) {\n"
                "                  const char *_mk; uint32_t _mkl;\n"
                "                  if (cb_unpack_str(&_r, &_mk, &_mkl) < 0) return -1;\n"
                "                  %s.entries[_mi].key = sstr_of(_mk, _mkl);\n",
                accessor, cb_map_suffix(f), cb_map_suffix(f),
                accessor, accessor);
            if (f->map_value_type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source,
                    "                  %s.entries[_mi].value = sstr_new();\n", accessor);
            }
            char val_acc[300];
            snprintf(val_acc, sizeof(val_acc), "%s.entries[_mi].value", accessor);
            cb_gen_unpack_map_value(f, val_acc, source);
            sstr_append_cstr(source, "              }\n            }\n");
        } else if (f->is_array) {
            if (f->array_size > 0) {
                /* fixed array */
                sstr_printf_append(source,
                    "            { uint32_t _ac;\n"
                    "              if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
                    "              for (uint32_t _ai = 0; _ai < _ac && _ai < %d; _ai++) {\n",
                    f->array_size);
            } else {
                /* dynamic array */
                const char *c_type;
                if (f->type == FIELD_TYPE_STRUCT) {
                    /* need "struct TypeName" */
                    sstr_t ct = sstr_new();
                    sstr_printf_append(ct, "struct %s", sstr_cstr(f->type_name));
                    sstr_printf_append(source,
                        "            { uint32_t _ac;\n"
                        "              if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
                        "              %s = (%s *)JGENC_MALLOC(sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              memset(%s, 0, sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              obj->%s_len = (int)_ac;\n"
                        "              for (uint32_t _ai = 0; _ai < _ac; _ai++) {\n",
                        accessor, sstr_cstr(ct), sstr_cstr(ct),
                        accessor, sstr_cstr(ct), sstr_cstr(f->name));
                    sstr_free(ct);
                    goto emit_elem;
                } else if (f->type == FIELD_TYPE_ONEOF) {
                    sstr_t ct = sstr_new();
                    sstr_printf_append(ct, "struct %s", sstr_cstr(f->type_name));
                    sstr_printf_append(source,
                        "            { uint32_t _ac;\n"
                        "              if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
                        "              %s = (%s *)JGENC_MALLOC(sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              memset(%s, 0, sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              obj->%s_len = (int)_ac;\n"
                        "              for (uint32_t _ai = 0; _ai < _ac; _ai++) {\n",
                        accessor, sstr_cstr(ct), sstr_cstr(ct),
                        accessor, sstr_cstr(ct), sstr_cstr(f->name));
                    sstr_free(ct);
                    goto emit_elem;
                } else if (f->type == FIELD_TYPE_MAP) {
                    sstr_t ct = sstr_new();
                    sstr_printf_append(ct, "struct map_container_%s", cb_map_suffix(f));
                    sstr_printf_append(source,
                        "            { uint32_t _ac;\n"
                        "              if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
                        "              %s = (%s *)JGENC_MALLOC(sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              memset(%s, 0, sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                        "              obj->%s_len = (int)_ac;\n"
                        "              for (uint32_t _ai = 0; _ai < _ac; _ai++) {\n",
                        accessor, sstr_cstr(ct), sstr_cstr(ct),
                        accessor, sstr_cstr(ct), sstr_cstr(f->name));
                    sstr_free(ct);
                    goto emit_elem;
                } else if (f->type == FIELD_TYPE_ENUM || f->type == FIELD_TYPE_BOOL) {
                    c_type = "int";
                } else if (f->type == FIELD_TYPE_SSTR) {
                    c_type = "sstr_t";
                } else {
                    c_type = sstr_cstr(f->type_name);
                }
                sstr_printf_append(source,
                    "            { uint32_t _ac;\n"
                    "              if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
                    "              %s = (%s *)JGENC_MALLOC(sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                    "              memset(%s, 0, sizeof(%s) * (_ac > 0 ? _ac : 1));\n"
                    "              obj->%s_len = (int)_ac;\n"
                    "              for (uint32_t _ai = 0; _ai < _ac; _ai++) {\n",
                    accessor, c_type, c_type,
                    accessor, c_type, sstr_cstr(f->name));
            }
emit_elem:;
            char elem_acc[300];
            snprintf(elem_acc, sizeof(elem_acc), "%s[_ai]", accessor);
            if (f->type == FIELD_TYPE_SSTR) {
                sstr_printf_append(source,
                    "                  %s = sstr_new();\n", elem_acc);
            }
            struct struct_field elem_f = *f;
            elem_f.is_array = 0;
            cb_gen_unpack_scalar(&elem_f, elem_acc, source);
            if (f->array_size > 0) {
                /* skip remaining if more than fixed size */
                sstr_printf_append(source,
                    "              }\n"
                    "              for (uint32_t _ai = %d; _ai < _ac; _ai++) cb_unpack_skip(&_r);\n"
                    "            }\n",
                    f->array_size);
            } else {
                sstr_append_cstr(source, "              }\n            }\n");
            }
        } else {
            if (f->type == FIELD_TYPE_SSTR) {
                /* sstr_t fields may need init if not yet allocated */
                sstr_printf_append(source,
                    "            if (%s == NULL) %s = sstr_new();\n",
                    accessor, accessor);
            }
            sstr_printf_append(source, "    ");
            cb_gen_unpack_scalar(f, accessor, source);
        }

        if (f->is_nullable) {
            sstr_append_cstr(source, "            }\n");  /* close else */
        }

        f = f->next;
    }

    if (!first) {
        sstr_append_cstr(source,
            "        } else {\n"
            "            cb_unpack_skip(&_r);\n"
            "        }\n");
    }

    sstr_append_cstr(source,
        "    }\n"
        "    return 0;\n}\n\n");
}

/* ── pack/unpack array wrappers ─────────────────────────────────────── */

static void cb_gen_pack_array(struct struct_container *st, sstr_t source) {
    sstr_printf_append(source,
        "int cbor_pack_array_%s(struct %s *obj, int count, sstr_t out) {\n"
        "    cb_pack_array_header(out, (uint32_t)count);\n"
        "    for (int _i = 0; _i < count; _i++) {\n"
        "        cbor_pack_%s(&obj[_i], out);\n"
        "    }\n"
        "    return 0;\n}\n\n",
        sstr_cstr(st->name), sstr_cstr(st->name), sstr_cstr(st->name));
}

static void cb_gen_unpack_array(struct struct_container *st, sstr_t source) {
    sstr_printf_append(source,
        "int cbor_unpack_array_%s(const unsigned char *data, size_t len, struct %s **obj, int *count) {\n"
        "    struct cb_reader _r;\n"
        "    cb_reader_init(&_r, data, len);\n"
        "    uint32_t _ac;\n"
        "    if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
        "    *obj = (struct %s *)JGENC_MALLOC(sizeof(struct %s) * (_ac > 0 ? _ac : 1));\n"
        "    if (!*obj) return -1;\n"
        "    memset(*obj, 0, sizeof(struct %s) * (_ac > 0 ? _ac : 1));\n"
        "    *count = (int)_ac;\n"
        "    for (uint32_t _i = 0; _i < _ac; _i++) {\n"
        "        %s_init(&(*obj)[_i]);\n"
        "        size_t _before = _r.pos;\n"
        "        if (cbor_unpack_%s(_r.data + _r.pos, _r.len - _r.pos, &(*obj)[_i]) < 0) {\n"
        "            for (uint32_t _j = 0; _j <= _i; _j++) %s_clear(&(*obj)[_j]);\n"
        "            JGENC_FREE(*obj); *obj = NULL; *count = 0;\n"
        "            return -1;\n"
        "        }\n"
        "        struct cb_reader _tmp; cb_reader_init(&_tmp, _r.data + _before, _r.len - _before);\n"
        "        if (cb_unpack_skip(&_tmp) < 0) return -1;\n"
        "        _r.pos = _before + _tmp.pos;\n"
        "    }\n"
        "    return 0;\n}\n\n",
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name),
        sstr_cstr(st->name),
        sstr_cstr(st->name),
        sstr_cstr(st->name), sstr_cstr(st->name));
}

/* ── oneof pack/unpack ──────────────────────────────────────────────── */

static void cb_gen_oneof_pack(struct oneof_container *oc, sstr_t source) {
    const char *tag_field = oc->tag_field ? sstr_cstr(oc->tag_field) : "type";

    sstr_printf_append(source,
        "int cbor_pack_%s(struct %s *obj, sstr_t out) {\n"
        "    /* Oneof: pack tag + variant fields flattened into one map */\n"
        "    /* First pack variant to temp buffer, then merge */\n"
        "    sstr_t _tmp = sstr_new();\n"
        "    switch (obj->tag) {\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name));

    struct oneof_variant *var = oc->variants;
    int i = 0;
    while (var) {
        sstr_printf_append(source,
            "    case %s_%s:\n"
            "        cbor_pack_%s(&obj->value.%s, _tmp);\n"
            "        break;\n",
            sstr_cstr(oc->name), sstr_cstr(var->name),
            sstr_cstr(var->struct_type_name), sstr_cstr(var->name));
        i++;
        var = var->next;
    }

    sstr_printf_append(source,
        "    default: break;\n"
        "    }\n"
        "    /* Count fields in the variant map */\n"
        "    struct cb_reader _cr;\n"
        "    cb_reader_init(&_cr, (const unsigned char *)sstr_cstr(_tmp), sstr_length(_tmp));\n"
        "    uint32_t _vc = 0;\n"
        "    if (sstr_length(_tmp) > 0) cb_unpack_map_header(&_cr, &_vc);\n"
        "    /* total = tag field + variant fields */\n"
        "    cb_pack_map_header(out, 1 + _vc);\n"
        "    /* pack tag */\n"
        "    cb_pack_str(out, \"%s\", %d);\n"
        "    if (obj->tag >= 0 && obj->tag < %s_tag_count) {\n"
        "        const char *_ts = %s_tag_strings[obj->tag];\n"
        "        cb_pack_str(out, _ts, (uint32_t)strlen(_ts));\n"
        "    } else {\n"
        "        cb_pack_int(out, (int64_t)obj->tag);\n"
        "    }\n"
        "    /* append variant fields (raw bytes after map header) */\n"
        "    if (sstr_length(_tmp) > 0 && _cr.pos < sstr_length(_tmp)) {\n"
        "        sstr_append_of(out, sstr_cstr(_tmp) + _cr.pos, sstr_length(_tmp) - _cr.pos);\n"
        "    }\n"
        "    sstr_free(_tmp);\n"
        "    return 0;\n}\n\n",
        tag_field, (int)strlen(tag_field),
        sstr_cstr(oc->name), sstr_cstr(oc->name));
}

static void cb_gen_oneof_unpack(struct oneof_container *oc, sstr_t source) {
    const char *tag_field = oc->tag_field ? sstr_cstr(oc->tag_field) : "type";
    int tag_len = (int)strlen(tag_field);

    sstr_printf_append(source,
        "int cbor_unpack_%s(const unsigned char *data, size_t len, struct %s *obj) {\n"
        "    /* Two-pass: first find tag to determine variant, then unpack variant */\n"
        "    struct cb_reader _r;\n"
        "    cb_reader_init(&_r, data, len);\n"
        "    uint32_t _nf;\n"
        "    if (cb_unpack_map_header(&_r, &_nf) < 0) return -1;\n"
        "    int _tag = -1;\n"
        "    /* Pass 1: find tag */\n"
        "    for (uint32_t _i = 0; _i < _nf; _i++) {\n"
        "        const char *_k; uint32_t _kl;\n"
        "        if (cb_unpack_str(&_r, &_k, &_kl) < 0) return -1;\n"
        "        if (_kl == %d && memcmp(_k, \"%s\", %d) == 0) {\n"
        "            const char *_ts; uint32_t _tsl;\n"
        "            if (cb_unpack_str(&_r, &_ts, &_tsl) < 0) return -1;\n"
        "            for (int _ti = 0; _ti < %s_tag_count; _ti++) {\n"
        "                if (_tsl == (uint32_t)strlen(%s_tag_strings[_ti]) &&\n"
        "                    memcmp(_ts, %s_tag_strings[_ti], _tsl) == 0) {\n"
        "                    _tag = _ti; break;\n"
        "                }\n"
        "            }\n"
        "        } else {\n"
        "            cb_unpack_skip(&_r);\n"
        "        }\n"
        "    }\n"
        "    if (_tag < 0) return -1;\n"
        "    obj->tag = (enum %s_tag)_tag;\n"
        "    /* Pass 2: unpack variant struct from full map */\n"
        "    switch (obj->tag) {\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        tag_len, tag_field, tag_len,
        sstr_cstr(oc->name), sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name));

    struct oneof_variant *var = oc->variants;
    while (var) {
        sstr_printf_append(source,
            "    case %s_%s:\n"
            "        %s_init(&obj->value.%s);\n"
            "        if (cbor_unpack_%s(data, len, &obj->value.%s) < 0) return -1;\n"
            "        break;\n",
            sstr_cstr(oc->name), sstr_cstr(var->name),
            sstr_cstr(var->struct_type_name), sstr_cstr(var->name),
            sstr_cstr(var->struct_type_name), sstr_cstr(var->name));
        var = var->next;
    }
    sstr_append_cstr(source,
        "    default: return -1;\n"
        "    }\n"
        "    return 0;\n}\n\n");
}

static void cb_gen_oneof_pack_array(struct oneof_container *oc, sstr_t source) {
    sstr_printf_append(source,
        "int cbor_pack_array_%s(struct %s *obj, int count, sstr_t out) {\n"
        "    cb_pack_array_header(out, (uint32_t)count);\n"
        "    for (int _i = 0; _i < count; _i++) {\n"
        "        cbor_pack_%s(&obj[_i], out);\n"
        "    }\n"
        "    return 0;\n}\n\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name), sstr_cstr(oc->name));
}

static void cb_gen_oneof_unpack_array(struct oneof_container *oc, sstr_t source) {
    sstr_printf_append(source,
        "int cbor_unpack_array_%s(const unsigned char *data, size_t len, struct %s **obj, int *count) {\n"
        "    struct cb_reader _r;\n"
        "    cb_reader_init(&_r, data, len);\n"
        "    uint32_t _ac;\n"
        "    if (cb_unpack_array_header(&_r, &_ac) < 0) return -1;\n"
        "    *obj = (struct %s *)JGENC_MALLOC(sizeof(struct %s) * (_ac > 0 ? _ac : 1));\n"
        "    if (!*obj) return -1;\n"
        "    memset(*obj, 0, sizeof(struct %s) * (_ac > 0 ? _ac : 1));\n"
        "    *count = (int)_ac;\n"
        "    for (uint32_t _i = 0; _i < _ac; _i++) {\n"
        "        %s_init(&(*obj)[_i]);\n"
        "        size_t _before = _r.pos;\n"
        "        if (cbor_unpack_%s(_r.data + _r.pos, _r.len - _r.pos, &(*obj)[_i]) < 0) {\n"
        "            for (uint32_t _j = 0; _j <= _i; _j++) %s_clear(&(*obj)[_j]);\n"
        "            JGENC_FREE(*obj); *obj = NULL; *count = 0;\n"
        "            return -1;\n"
        "        }\n"
        "        struct cb_reader _tmp; cb_reader_init(&_tmp, _r.data + _before, _r.len - _before);\n"
        "        if (cb_unpack_skip(&_tmp) < 0) return -1;\n"
        "        _r.pos = _before + _tmp.pos;\n"
        "    }\n"
        "    return 0;\n}\n\n",
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name),
        sstr_cstr(oc->name),
        sstr_cstr(oc->name),
        sstr_cstr(oc->name), sstr_cstr(oc->name));
}

/* ── per-struct / per-oneof codegen ─────────────────────────────────── */

static void cb_gen_code_struct(struct struct_container *st, sstr_t source,
                               sstr_t header) {
    cb_gen_struct_header(st, header);
    cb_gen_struct_init(st, source);
    cb_gen_struct_clear(st, source);
    cb_gen_pack_struct(st, source);
    cb_gen_unpack_struct(st, source);
    cb_gen_pack_array(st, source);
    cb_gen_unpack_array(st, source);
}

static void cb_gen_code_oneof(struct oneof_container *oc, sstr_t source,
                              sstr_t header) {
    cb_gen_oneof_header(oc, header);
    cb_gen_oneof_init(oc, source);
    cb_gen_oneof_clear(oc, source);
    cb_gen_oneof_pack(oc, source);
    cb_gen_oneof_unpack(oc, source);
    cb_gen_oneof_pack_array(oc, source);
    cb_gen_oneof_unpack_array(oc, source);
}

/* ── dependency-order iteration (same pattern as JSON gencode) ──────── */

static void mp_dummy_free(void *ptr) { (void)ptr; }

struct cb_gen_struct_param {
    struct hash_map *dependency_map;
    sstr_t source;
    sstr_t header;
};

static void cb_do_each_struct(void *key, void *value, void *ptr) {
    sstr_t k = (sstr_t)key;
    struct struct_container *st = (struct struct_container *)value;
    struct cb_gen_struct_param *p = (struct cb_gen_struct_param *)ptr;

    void *dv = NULL;
    if (hash_map_find(p->dependency_map, k, &dv) == HASH_MAP_OK) return;

    struct struct_field *f = st->fields;
    while (f) {
        if (f->type == FIELD_TYPE_STRUCT || (f->type == FIELD_TYPE_MAP && f->map_value_type == FIELD_TYPE_STRUCT) || f->type == FIELD_TYPE_ONEOF) {
            sstr_t dep = (f->type == FIELD_TYPE_MAP) ? f->map_value_type_name : f->type_name;
            if (hash_map_find(p->dependency_map, dep, &dv) != HASH_MAP_OK) return;
        }
        f = f->next;
    }
    cb_gen_code_struct(st, p->source, p->header);
    hash_map_insert(p->dependency_map, sstr_dup(k), NULL);
}

struct cb_gen_oneof_param {
    struct hash_map *dependency_map;
    struct hash_map *struct_map;
    sstr_t source;
    sstr_t header;
};

static void cb_do_each_oneof(void *key, void *value, void *ptr) {
    sstr_t k = (sstr_t)key;
    struct oneof_container *oc = (struct oneof_container *)value;
    struct cb_gen_oneof_param *p = (struct cb_gen_oneof_param *)ptr;

    void *dv = NULL;
    if (hash_map_find(p->dependency_map, k, &dv) == HASH_MAP_OK) return;

    struct oneof_variant *var = oc->variants;
    while (var) {
        if (hash_map_find(p->dependency_map, var->struct_type_name, &dv) != HASH_MAP_OK) return;
        var = var->next;
    }
    cb_gen_code_oneof(oc, p->source, p->header);
    hash_map_insert(p->dependency_map, sstr_dup(k), NULL);
}

/* ── header preamble/epilogue ───────────────────────────────────────── */

static void cb_head_begin(sstr_t head) {
    sstr_append_cstr(head,
        "#ifndef CBOR_GEN_C_H__\n#define CBOR_GEN_C_H__\n\n"
        "#include \"sstr.h\"\n"
        "#include <stdbool.h>\n"
        "#include <stdint.h>\n"
        "#include <string.h>\n"
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");

    /* Deprecation macros (same as JSON header) */
    sstr_append_cstr(head,
        "#ifndef JGENC_DEPRECATED\n"
        "  #if defined(__GNUC__) || defined(__clang__)\n"
        "    #define JGENC_DEPRECATED(msg) __attribute__((deprecated(msg)))\n"
        "  #elif defined(_MSC_VER)\n"
        "    #define JGENC_DEPRECATED(msg) __declspec(deprecated(msg))\n"
        "  #else\n"
        "    #define JGENC_DEPRECATED(msg)\n"
        "  #endif\n"
        "#endif\n\n"
        "#ifndef JGENC_DEPRECATED_ENUM\n"
        "  #if defined(__GNUC__) || defined(__clang__)\n"
        "    #define JGENC_DEPRECATED_ENUM(msg) __attribute__((deprecated(msg)))\n"
        "  #else\n"
        "    #define JGENC_DEPRECATED_ENUM(msg)\n"
        "  #endif\n"
        "#endif\n\n");

    /* Allocator API */
    sstr_append_cstr(head,
        "#ifndef JGENC_MALLOC\n"
        "void json_gen_c_set_alloc(void* (*malloc_fn)(size_t),\n"
        "                          void* (*realloc_fn)(void*, size_t),\n"
        "                          void  (*free_fn)(void*));\n"
        "#endif\n\n");

    /* Field mask macros (same as JSON) */
    sstr_append_cstr(head,
        "#define JSON_GEN_C_FIELD_MASK_WORD_COUNT(field_count) \\\n"
        "    (((field_count) <= 0) ? 1 : (((field_count) + 63) / 64))\n"
        "#define JSON_GEN_C_FIELD_MASK_SET(mask_words, field_index) \\\n"
        "    ((mask_words)[(field_index) / 64u] |= \\\n"
        "        (UINT64_C(1) << ((field_index) % 64u)))\n"
        "#define JSON_GEN_C_FIELD_MASK_CLEAR(mask_words, field_index) \\\n"
        "    ((mask_words)[(field_index) / 64u] &= \\\n"
        "        ~(UINT64_C(1) << ((field_index) % 64u)))\n"
        "#define JSON_GEN_C_FIELD_MASK_TEST(mask_words, field_index) \\\n"
        "    (((mask_words)[(field_index) / 64u] & \\\n"
        "        (UINT64_C(1) << ((field_index) % 64u))) != 0)\n\n");
}

static void cb_head_end(sstr_t head) {
    sstr_append_cstr(head,
        "#ifdef __cplusplus\n}\n#endif\n\n"
        "#endif /* CBOR_GEN_C_H__ */\n");
}

/* ── source preamble/epilogue ───────────────────────────────────────── */

#include "extra_codes_cbor.inc"

static void cb_source_begin(sstr_t source) {
    sstr_append_cstr(source,
        "#include \"cbor.gen.h\"\n\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <stdint.h>\n\n");

    /* Suppress GCC deprecation warnings inside generated code */
    sstr_append_cstr(source,
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "#pragma GCC diagnostic ignored \"-Wdeprecated-declarations\"\n"
        "#endif\n\n");

    /* Allocator macros (same as JSON) */
    sstr_append_cstr(source,
        "#ifndef JGENC_MALLOC\n"
        "static void* (*jgenc_malloc_fn_)(size_t) = NULL;\n"
        "static void* (*jgenc_realloc_fn_)(void*, size_t) = NULL;\n"
        "static void  (*jgenc_free_fn_)(void*) = NULL;\n\n"
        "static inline void* jgenc_malloc_dispatch_(size_t s) {\n"
        "    return jgenc_malloc_fn_ ? jgenc_malloc_fn_(s) : malloc(s);\n"
        "}\n"
        "static inline void* jgenc_realloc_dispatch_(void *p, size_t s) {\n"
        "    return jgenc_realloc_fn_ ? jgenc_realloc_fn_(p, s) : realloc(p, s);\n"
        "}\n"
        "static inline void jgenc_free_dispatch_(void *p) {\n"
        "    jgenc_free_fn_ ? jgenc_free_fn_(p) : free(p);\n"
        "}\n\n"
        "#define JGENC_MALLOC(s)    jgenc_malloc_dispatch_(s)\n"
        "#define JGENC_REALLOC(p,s) jgenc_realloc_dispatch_((p),(s))\n"
        "#define JGENC_FREE(p)      jgenc_free_dispatch_(p)\n\n"
        "void json_gen_c_set_alloc(void* (*malloc_fn)(size_t),\n"
        "                          void* (*realloc_fn)(void*, size_t),\n"
        "                          void  (*free_fn)(void*)) {\n"
        "    jgenc_malloc_fn_  = malloc_fn;\n"
        "    jgenc_realloc_fn_ = realloc_fn;\n"
        "    jgenc_free_fn_    = free_fn;\n"
        "}\n"
        "#else\n"
        "#ifndef JGENC_REALLOC\n#define JGENC_REALLOC(p,s) realloc((p),(s))\n#endif\n"
        "#ifndef JGENC_FREE\n#define JGENC_FREE(p) free(p)\n#endif\n"
        "#endif\n\n");

    /* Embed cbor_codec.h + cbor_codec.c */
    sstr_append_of(source, (const char *)cbor_codec_h,
                   (size_t)cbor_codec_h_len);
    sstr_append_cstr(source, "\n");
    sstr_append_of(source, (const char *)cbor_codec_c,
                   (size_t)cbor_codec_c_len);
    sstr_append_cstr(source, "\n");
}

/* ── public entry point ─────────────────────────────────────────────── */

int gencode_cbor_source(struct hash_map *struct_map, struct hash_map *enum_map,
                           struct hash_map *oneof_map, sstr_t source, sstr_t header) {
    struct hash_map *dep_map = hash_map_new(DEPENDENCY_HASH_MAP_BUCKET_SIZE,
        sstr_key_hash, sstr_key_cmp, sstr_key_free, mp_dummy_free);
    if (!dep_map) return -1;

    /* header preamble */
    cb_head_begin(header);

    /* enum headers */
    hash_map_for_each(enum_map, cb_gen_enum_header_fn, header);

    /* source preamble (includes embedded runtime) */
    cb_source_begin(source);

    /* enum string arrays */
    hash_map_for_each(enum_map, cb_gen_enum_string_fn, source);

    /* oneof statics */
    hash_map_for_each(oneof_map, cb_gen_oneof_statics_fn, source);

    /* Generate structs + oneofs in dependency order */
    struct cb_gen_struct_param sp = { dep_map, source, header };
    struct cb_gen_oneof_param op = { dep_map, struct_map, source, header };
    int total = struct_map->size + oneof_map->size;

    for (int i = 0; i < total; i++) {
        hash_map_for_each(struct_map, cb_do_each_struct, &sp);
        hash_map_for_each(oneof_map, cb_do_each_oneof, &op);
        if (dep_map->size >= total) break;
    }
    if (dep_map->size < total) {
        fprintf(stderr, "struct/oneof dependency circle detected\n");
        hash_map_free(dep_map);
        return -1;
    }

    /* header epilogue */
    cb_head_end(header);

    hash_map_free(dep_map);
    return 0;
}
