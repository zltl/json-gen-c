/**
 * @file gencode_go.c
 * @brief Generate a Go source file (.go) with native encoding/json-compatible
 * structs and type-safe enums from the parsed schema. The output is a single
 * self-contained Go file using only the standard library.
 */

#include "gencode/gencode.h"
#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================= */
/* Type mapping                                                              */
/* ========================================================================= */

static const char* go_type_for(int field_type) {
    switch (field_type) {
        case FIELD_TYPE_INT:    return "int32";
        case FIELD_TYPE_LONG:   return "int64";
        case FIELD_TYPE_FLOAT:  return "float32";
        case FIELD_TYPE_DOUBLE: return "float64";
        case FIELD_TYPE_BOOL:   return "bool";
        case FIELD_TYPE_SSTR:   return "string";
        case FIELD_TYPE_INT8:   return "int8";
        case FIELD_TYPE_INT16:  return "int16";
        case FIELD_TYPE_INT32:  return "int32";
        case FIELD_TYPE_INT64:  return "int64";
        case FIELD_TYPE_UINT8:  return "uint8";
        case FIELD_TYPE_UINT16: return "uint16";
        case FIELD_TYPE_UINT32: return "uint32";
        case FIELD_TYPE_UINT64: return "uint64";
        default:                return NULL;
    }
}

/* ========================================================================= */
/* Name helpers                                                              */
/* ========================================================================= */

/** Convert first character to uppercase (Go exported name). */
static sstr_t go_exported_name(const char* name) {
    sstr_t out = sstr_new();
    if (name[0]) {
        char upper = (char)toupper(name[0]);
        sstr_append_of(out, &upper, 1);
        sstr_append_cstr(out, name + 1);
    }
    return out;
}

/** Get the JSON key name for a field (alias or field name). */
static const char* json_key(struct struct_field* f) {
    if (f->json_name && sstr_length(f->json_name) > 0)
        return sstr_cstr(f->json_name);
    return sstr_cstr(f->name);
}

/** Build the full Go type string for a field (without pointer wrapping). */
static sstr_t go_field_type(struct struct_field* f) {
    sstr_t ty = sstr_new();

    if (f->type == FIELD_TYPE_MAP) {
        const char* val_ty = go_type_for(f->map_value_type);
        sstr_append_cstr(ty, "map[string]");
        if (val_ty) {
            sstr_append_cstr(ty, val_ty);
        } else {
            sstr_append_cstr(ty, sstr_cstr(f->map_value_type_name));
        }
    } else {
        const char* simple = go_type_for(f->type);
        if (simple) {
            sstr_append_cstr(ty, simple);
        } else {
            /* enum, struct, or oneof — use type name directly */
            sstr_append_cstr(ty, sstr_cstr(f->type_name));
        }
    }

    /* Wrap in array/slice if needed */
    if (f->is_array) {
        sstr_t wrapped = sstr_new();
        if (f->array_size > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "[%d]", f->array_size);
            sstr_append_cstr(wrapped, buf);
        } else {
            sstr_append_cstr(wrapped, "[]");
        }
        sstr_append(wrapped, ty);
        sstr_free(ty);
        ty = wrapped;
    }

    return ty;
}

/* ========================================================================= */
/* Emit enums                                                                */
/* ========================================================================= */

struct go_emit_ctx {
    sstr_t output;
    struct hash_map* enum_map;
    struct hash_map* oneof_map;
    struct hash_map* dep_map;
};

static void emit_enum(void* key, void* value, void* ctx_ptr) {
    (void)key;
    struct enum_container* ec = (struct enum_container*)value;
    sstr_t out = ((struct go_emit_ctx*)ctx_ptr)->output;
    const char* ename = sstr_cstr(ec->name);

    /* type Color string */
    sstr_append_cstr(out, "type ");
    sstr_append_cstr(out, ename);
    sstr_append_cstr(out, " string\n\n");

    /* const block */
    sstr_append_cstr(out, "const (\n");
    struct enum_value* ev = ec->values;
    while (ev) {
        sstr_append_cstr(out, "\t");
        sstr_append_cstr(out, ename);
        sstr_append(out, ev->name);
        sstr_append_cstr(out, " ");
        sstr_append_cstr(out, ename);
        sstr_append_cstr(out, " = \"");
        sstr_append(out, ev->name);
        sstr_append_cstr(out, "\"\n");
        ev = ev->next;
    }
    sstr_append_cstr(out, ")\n\n");
}

/* ========================================================================= */
/* Emit structs                                                              */
/* ========================================================================= */

static void emit_struct(struct struct_container* sc, struct go_emit_ctx* ctx) {
    sstr_t out = ctx->output;

    sstr_append_cstr(out, "type ");
    sstr_append(out, sc->name);
    sstr_append_cstr(out, " struct {\n");

    struct struct_field* f = sc->fields;
    while (f) {
        int needs_ptr = f->is_optional || f->is_nullable;
        const char* jkey = json_key(f);

        /* Field name (exported) */
        sstr_t fname = go_exported_name(sstr_cstr(f->name));
        sstr_append_cstr(out, "\t");
        sstr_append(out, fname);
        sstr_append_cstr(out, " ");
        sstr_free(fname);

        /* Type */
        sstr_t ty = go_field_type(f);
        if (needs_ptr && !f->is_array && f->type != FIELD_TYPE_MAP) {
            sstr_append_cstr(out, "*");
        }
        sstr_append(out, ty);
        sstr_free(ty);

        /* JSON struct tag */
        sstr_append_cstr(out, " `json:\"");
        sstr_append_cstr(out, jkey);
        if (f->is_optional) {
            sstr_append_cstr(out, ",omitempty");
        }
        sstr_append_cstr(out, "\"`");

        sstr_append_cstr(out, "\n");
        f = f->next;
    }

    sstr_append_cstr(out, "}\n\n");
}

/* ========================================================================= */
/* Emit default constructors                                                 */
/* ========================================================================= */

static int struct_has_defaults(struct struct_container* sc) {
    struct struct_field* f = sc->fields;
    while (f) {
        if (f->has_default) return 1;
        f = f->next;
    }
    return 0;
}

static void emit_default_func(struct struct_container* sc, struct go_emit_ctx* ctx) {
    if (!struct_has_defaults(sc)) return;
    sstr_t out = ctx->output;
    const char* sname = sstr_cstr(sc->name);

    sstr_append_cstr(out, "// New");
    sstr_append_cstr(out, sname);
    sstr_append_cstr(out, " creates a ");
    sstr_append_cstr(out, sname);
    sstr_append_cstr(out, " with default values.\n");
    sstr_append_cstr(out, "func New");
    sstr_append_cstr(out, sname);
    sstr_append_cstr(out, "() ");
    sstr_append_cstr(out, sname);
    sstr_append_cstr(out, " {\n");
    sstr_append_cstr(out, "\treturn ");
    sstr_append_cstr(out, sname);
    sstr_append_cstr(out, "{\n");

    struct struct_field* f = sc->fields;
    while (f) {
        if (f->has_default) {
            sstr_t fname = go_exported_name(sstr_cstr(f->name));
            sstr_append_cstr(out, "\t\t");
            sstr_append(out, fname);
            sstr_append_cstr(out, ": ");
            sstr_free(fname);

            const char* dv = sstr_cstr(f->default_value);
            int is_opt = f->is_optional || f->is_nullable;

            if (f->type == FIELD_TYPE_SSTR) {
                if (is_opt) {
                    sstr_append_cstr(out, "ptrString(\"");
                } else {
                    sstr_append_cstr(out, "\"");
                }
                sstr_append_cstr(out, dv);
                if (is_opt) {
                    sstr_append_cstr(out, "\")");
                } else {
                    sstr_append_cstr(out, "\"");
                }
            } else if (f->type == FIELD_TYPE_BOOL) {
                if (is_opt) {
                    sstr_append_cstr(out, "ptrBool(");
                    sstr_append_cstr(out, dv);
                    sstr_append_cstr(out, ")");
                } else {
                    sstr_append_cstr(out, dv);
                }
            } else if (f->type == FIELD_TYPE_ENUM) {
                if (is_opt) {
                    sstr_append_cstr(out, "ptr");
                    sstr_append_cstr(out, sstr_cstr(f->type_name));
                    sstr_append_cstr(out, "(");
                }
                sstr_append_cstr(out, sstr_cstr(f->type_name));
                sstr_append_cstr(out, dv);
                if (is_opt) sstr_append_cstr(out, ")");
            } else if (f->type == FIELD_TYPE_FLOAT || f->type == FIELD_TYPE_DOUBLE) {
                if (is_opt) {
                    sstr_append_cstr(out, f->type == FIELD_TYPE_FLOAT ? "ptrFloat32(" : "ptrFloat64(");
                    sstr_append_cstr(out, dv);
                    sstr_append_cstr(out, ")");
                } else {
                    sstr_append_cstr(out, dv);
                }
            } else {
                /* integer types */
                if (is_opt) {
                    const char* gty = go_type_for(f->type);
                    sstr_append_cstr(out, "ptr");
                    /* Capitalize go type for helper name */
                    char upper = (char)toupper(gty[0]);
                    sstr_append_of(out, &upper, 1);
                    sstr_append_cstr(out, gty + 1);
                    sstr_append_cstr(out, "(");
                    sstr_append_cstr(out, dv);
                    sstr_append_cstr(out, ")");
                } else {
                    sstr_append_cstr(out, dv);
                }
            }

            sstr_append_cstr(out, ",\n");
        }
        f = f->next;
    }

    sstr_append_cstr(out, "\t}\n");
    sstr_append_cstr(out, "}\n\n");
}

/* ========================================================================= */
/* Emit oneofs (tagged unions)                                               */
/* ========================================================================= */

static void emit_oneof(struct oneof_container* oc, struct go_emit_ctx* ctx) {
    sstr_t out = ctx->output;
    const char* oname = sstr_cstr(oc->name);
    const char* tag = sstr_cstr(oc->tag_field);

    /* Wrapper struct */
    sstr_append_cstr(out, "type ");
    sstr_append_cstr(out, oname);
    sstr_append_cstr(out, " struct {\n");
    sstr_append_cstr(out, "\tTag   string      `json:\"-\"`\n");

    struct oneof_variant* v = oc->variants;
    while (v) {
        sstr_t fname = go_exported_name(sstr_cstr(v->name));
        sstr_append_cstr(out, "\t");
        sstr_append(out, fname);
        sstr_append_cstr(out, " *");
        sstr_append(out, v->struct_type_name);
        sstr_append_cstr(out, " `json:\"-\"`\n");
        sstr_free(fname);
        v = v->next;
    }
    sstr_append_cstr(out, "}\n\n");

    /* MarshalJSON */
    sstr_append_cstr(out, "func (o ");
    sstr_append_cstr(out, oname);
    sstr_append_cstr(out, ") MarshalJSON() ([]byte, error) {\n");
    sstr_append_cstr(out, "\tswitch o.Tag {\n");

    v = oc->variants;
    while (v) {
        sstr_t fname = go_exported_name(sstr_cstr(v->name));
        sstr_append_cstr(out, "\tcase \"");
        sstr_append(out, v->name);
        sstr_append_cstr(out, "\":\n");
        sstr_append_cstr(out, "\t\tif o.");
        sstr_append(out, fname);
        sstr_append_cstr(out, " == nil {\n");
        sstr_append_cstr(out, "\t\t\treturn json.Marshal(map[string]string{\"");
        sstr_append_cstr(out, tag);
        sstr_append_cstr(out, "\": \"");
        sstr_append(out, v->name);
        sstr_append_cstr(out, "\"})\n");
        sstr_append_cstr(out, "\t\t}\n");

        /* Marshal variant fields flattened with tag */
        sstr_append_cstr(out, "\t\ttype Alias ");
        sstr_append(out, v->struct_type_name);
        sstr_append_cstr(out, "\n");
        sstr_append_cstr(out, "\t\treturn json.Marshal(&struct {\n");
        sstr_append_cstr(out, "\t\t\tType string `json:\"");
        sstr_append_cstr(out, tag);
        sstr_append_cstr(out, "\"`\n");
        sstr_append_cstr(out, "\t\t\t*Alias\n");
        sstr_append_cstr(out, "\t\t}{Type: \"");
        sstr_append(out, v->name);
        sstr_append_cstr(out, "\", Alias: (*Alias)(o.");
        sstr_append(out, fname);
        sstr_append_cstr(out, ")})\n");

        sstr_free(fname);
        v = v->next;
    }

    sstr_append_cstr(out, "\t}\n");
    sstr_append_cstr(out, "\treturn nil, fmt.Errorf(\"unknown ");
    sstr_append_cstr(out, oname);
    sstr_append_cstr(out, " tag: %s\", o.Tag)\n");
    sstr_append_cstr(out, "}\n\n");

    /* UnmarshalJSON */
    sstr_append_cstr(out, "func (o *");
    sstr_append_cstr(out, oname);
    sstr_append_cstr(out, ") UnmarshalJSON(data []byte) error {\n");
    sstr_append_cstr(out, "\tvar raw struct {\n");
    sstr_append_cstr(out, "\t\tType string `json:\"");
    sstr_append_cstr(out, tag);
    sstr_append_cstr(out, "\"`\n");
    sstr_append_cstr(out, "\t}\n");
    sstr_append_cstr(out, "\tif err := json.Unmarshal(data, &raw); err != nil {\n");
    sstr_append_cstr(out, "\t\treturn err\n");
    sstr_append_cstr(out, "\t}\n");
    sstr_append_cstr(out, "\to.Tag = raw.Type\n");
    sstr_append_cstr(out, "\tswitch raw.Type {\n");

    v = oc->variants;
    while (v) {
        sstr_t fname = go_exported_name(sstr_cstr(v->name));
        sstr_append_cstr(out, "\tcase \"");
        sstr_append(out, v->name);
        sstr_append_cstr(out, "\":\n");
        sstr_append_cstr(out, "\t\to.");
        sstr_append(out, fname);
        sstr_append_cstr(out, " = &");
        sstr_append(out, v->struct_type_name);
        sstr_append_cstr(out, "{}\n");
        sstr_append_cstr(out, "\t\treturn json.Unmarshal(data, o.");
        sstr_append(out, fname);
        sstr_append_cstr(out, ")\n");
        sstr_free(fname);
        v = v->next;
    }

    sstr_append_cstr(out, "\tdefault:\n");
    sstr_append_cstr(out, "\t\treturn fmt.Errorf(\"unknown ");
    sstr_append_cstr(out, oname);
    sstr_append_cstr(out, " tag: %s\", raw.Type)\n");
    sstr_append_cstr(out, "\t}\n");
    sstr_append_cstr(out, "}\n\n");
}

/* ========================================================================= */
/* Pointer helper functions for default values                               */
/* ========================================================================= */

static void emit_ptr_helpers(sstr_t out, struct hash_map* struct_map) {
    /* Track which helpers are needed */
    int need_string = 0, need_bool = 0;
    int need_int32 = 0, need_int64 = 0;
    int need_float32 = 0, need_float64 = 0;
    int need_int8 = 0, need_int16 = 0;
    int need_uint8 = 0, need_uint16 = 0, need_uint32 = 0, need_uint64 = 0;

    /* Scan all structs for optional/nullable fields with defaults */
    /* For simplicity, just emit the common set */
    (void)struct_map;
    (void)need_string; (void)need_bool;
    (void)need_int32; (void)need_int64;
    (void)need_float32; (void)need_float64;
    (void)need_int8; (void)need_int16;
    (void)need_uint8; (void)need_uint16; (void)need_uint32; (void)need_uint64;

    sstr_append_cstr(out,
        "// Pointer helpers for optional default values.\n"
        "func ptrString(v string) *string     { return &v }\n"
        "func ptrBool(v bool) *bool           { return &v }\n"
        "func ptrInt32(v int32) *int32        { return &v }\n"
        "func ptrInt64(v int64) *int64        { return &v }\n"
        "func ptrFloat32(v float32) *float32  { return &v }\n"
        "func ptrFloat64(v float64) *float64  { return &v }\n"
        "func ptrInt8(v int8) *int8           { return &v }\n"
        "func ptrInt16(v int16) *int16        { return &v }\n"
        "func ptrUint8(v uint8) *uint8        { return &v }\n"
        "func ptrUint16(v uint16) *uint16     { return &v }\n"
        "func ptrUint32(v uint32) *uint32     { return &v }\n"
        "func ptrUint64(v uint64) *uint64     { return &v }\n\n");
}

/* ========================================================================= */
/* Dependency-ordered emission                                               */
/* ========================================================================= */

static void dummy_free(void* ptr) { (void)ptr; }

static int is_emitted(struct hash_map* dep_map, const char* name) {
    void* v = NULL;
    sstr_t key = sstr(name);
    int found = hash_map_find(dep_map, key, &v) == HASH_MAP_OK;
    sstr_free(key);
    return found;
}

static void mark_emitted(struct hash_map* dep_map, const char* name) {
    hash_map_insert(dep_map, sstr(name), NULL);
}

static int struct_deps_ready(struct struct_container* sc, struct hash_map* dep_map) {
    struct struct_field* f = sc->fields;
    while (f) {
        if (f->type == FIELD_TYPE_STRUCT || f->type == FIELD_TYPE_ONEOF) {
            if (!is_emitted(dep_map, sstr_cstr(f->type_name)))
                return 0;
        }
        if (f->type == FIELD_TYPE_MAP &&
            (f->map_value_type == FIELD_TYPE_STRUCT ||
             f->map_value_type == FIELD_TYPE_ONEOF)) {
            if (!is_emitted(dep_map, sstr_cstr(f->map_value_type_name)))
                return 0;
        }
        f = f->next;
    }
    return 1;
}

static int oneof_deps_ready(struct oneof_container* oc, struct hash_map* dep_map) {
    struct oneof_variant* v = oc->variants;
    while (v) {
        if (!is_emitted(dep_map, sstr_cstr(v->struct_type_name)))
            return 0;
        v = v->next;
    }
    return 1;
}

struct dep_emit_ctx {
    struct go_emit_ctx* gen;
    int emitted_any;
};

static void try_emit_struct(void* key, void* value, void* ctx_ptr) {
    sstr_t k = (sstr_t)key;
    struct struct_container* sc = (struct struct_container*)value;
    struct dep_emit_ctx* ctx = (struct dep_emit_ctx*)ctx_ptr;

    if (is_emitted(ctx->gen->dep_map, sstr_cstr(k))) return;
    if (!struct_deps_ready(sc, ctx->gen->dep_map)) return;

    emit_struct(sc, ctx->gen);
    emit_default_func(sc, ctx->gen);
    mark_emitted(ctx->gen->dep_map, sstr_cstr(k));
    ctx->emitted_any = 1;
}

static void try_emit_oneof(void* key, void* value, void* ctx_ptr) {
    sstr_t k = (sstr_t)key;
    struct oneof_container* oc = (struct oneof_container*)value;
    struct dep_emit_ctx* ctx = (struct dep_emit_ctx*)ctx_ptr;

    if (is_emitted(ctx->gen->dep_map, sstr_cstr(k))) return;
    if (!oneof_deps_ready(oc, ctx->gen->dep_map)) return;

    emit_oneof(oc, ctx->gen);
    mark_emitted(ctx->gen->dep_map, sstr_cstr(k));
    ctx->emitted_any = 1;
}

static void mark_enum_emitted(void* key, void* value, void* ctx_ptr) {
    (void)value;
    sstr_t name = (sstr_t)key;
    struct hash_map* dep_map = (struct hash_map*)ctx_ptr;
    mark_emitted(dep_map, sstr_cstr(name));
}

/* ========================================================================= */
/* Public entry point                                                        */
/* ========================================================================= */

int gencode_go(struct hash_map* struct_map, struct hash_map* enum_map,
               struct hash_map* oneof_map, const char* package_name,
               sstr_t output) {
    struct hash_map* dep_map =
        hash_map_new(256, sstr_key_hash, sstr_key_cmp, sstr_key_free,
                     dummy_free);
    if (dep_map == NULL) return -1;

    struct go_emit_ctx ctx;
    ctx.output = output;
    ctx.enum_map = enum_map;
    ctx.oneof_map = oneof_map;
    ctx.dep_map = dep_map;

    /* File header */
    sstr_append_cstr(output,
        "// Auto-generated Go source — DO NOT EDIT.\n"
        "// Generated by json-gen-c with --go.\n\n");
    sstr_append_cstr(output, "package ");
    sstr_append_cstr(output, package_name);
    sstr_append_cstr(output, "\n\n");

    /* Imports — always include encoding/json and fmt for oneofs */
    int has_oneofs = oneof_map->size > 0;
    sstr_append_cstr(output, "import (\n");
    sstr_append_cstr(output, "\t\"encoding/json\"\n");
    if (has_oneofs) {
        sstr_append_cstr(output, "\t\"fmt\"\n");
    }
    sstr_append_cstr(output, ")\n\n");

    /* Ensure json is used even without oneofs (struct tags use it implicitly,
       but the import is needed for tests that call json.Marshal). We suppress
       the unused import warning with a blank identifier if no oneofs. */
    if (!has_oneofs) {
        sstr_append_cstr(output, "var _ = json.Marshal // ensure import\n\n");
    }

    /* Pointer helpers for optional defaults */
    emit_ptr_helpers(output, struct_map);

    /* 1) Emit enums */
    hash_map_for_each(enum_map, emit_enum, &ctx);
    hash_map_for_each(enum_map, mark_enum_emitted, dep_map);

    /* 2) Emit structs and oneofs in dependency order */
    int total = (int)struct_map->size + (int)oneof_map->size;
    for (int round = 0; round < total; ++round) {
        struct dep_emit_ctx dctx;
        dctx.gen = &ctx;
        dctx.emitted_any = 0;

        hash_map_for_each(struct_map, try_emit_struct, &dctx);
        hash_map_for_each(oneof_map, try_emit_oneof, &dctx);

        if ((int)dep_map->size >= total + (int)enum_map->size) break;
        if (!dctx.emitted_any) break;
    }

    int emitted_total = (int)dep_map->size - (int)enum_map->size;
    hash_map_free(dep_map);

    if (emitted_total < total) {
        fprintf(stderr, "Error: could not resolve all type dependencies for Go generation\n");
        return -1;
    }

    return 0;
}
