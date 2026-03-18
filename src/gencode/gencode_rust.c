/**
 * @file gencode_rust.c
 * @brief Generate a Rust module (.rs) with native serde-compatible structs
 * and enums from the parsed schema. The output is a single self-contained
 * Rust file that uses serde + serde_json for JSON serialization.
 */

#include "gencode/gencode.h"
#include "struct/struct_parse.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================= */
/* Type mapping                                                              */
/* ========================================================================= */

static const char* rust_type_for(int field_type) {
    switch (field_type) {
        case FIELD_TYPE_INT:    return "i32";
        case FIELD_TYPE_LONG:   return "i64";
        case FIELD_TYPE_FLOAT:  return "f32";
        case FIELD_TYPE_DOUBLE: return "f64";
        case FIELD_TYPE_BOOL:   return "bool";
        case FIELD_TYPE_SSTR:   return "String";
        case FIELD_TYPE_INT8:   return "i8";
        case FIELD_TYPE_INT16:  return "i16";
        case FIELD_TYPE_INT32:  return "i32";
        case FIELD_TYPE_INT64:  return "i64";
        case FIELD_TYPE_UINT8:  return "u8";
        case FIELD_TYPE_UINT16: return "u16";
        case FIELD_TYPE_UINT32: return "u32";
        case FIELD_TYPE_UINT64: return "u64";
        default:                return NULL;
    }
}

static int is_numeric_type(int ft) {
    return (ft >= FIELD_TYPE_INT && ft <= FIELD_TYPE_DOUBLE)
        || (ft >= FIELD_TYPE_INT8 && ft <= FIELD_TYPE_UINT64);
}

/* ========================================================================= */
/* String helpers                                                            */
/* ========================================================================= */

/** Convert a C identifier to snake_case (simple: insert _ before uppercase). */
static sstr_t to_snake_case(const char* name) {
    sstr_t out = sstr_new();
    for (int i = 0; name[i]; ++i) {
        char c = name[i];
        if (isupper(c) && i > 0 && (islower(name[i-1]) || (name[i+1] && islower(name[i+1])))) {
            sstr_append_of(out, "_", 1);
        }
        char lc = (char)tolower(c);
        sstr_append_of(out, &lc, 1);
    }
    return out;
}

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

/** Get the JSON key name for a field (alias or field name). */
static const char* json_key(struct struct_field* f) {
    if (f->json_name && sstr_length(f->json_name) > 0) {
        return sstr_cstr(f->json_name);
    }
    return sstr_cstr(f->name);
}

/** Return the full Rust type string for a field (without Option wrapping). */
static sstr_t rust_field_type(struct struct_field* f, struct hash_map* oneof_map) {
    sstr_t ty = sstr_new();

    if (f->type == FIELD_TYPE_MAP) {
        const char* val_ty = rust_type_for(f->map_value_type);
        sstr_append_cstr(ty, "HashMap<String, ");
        if (val_ty) {
            sstr_append_cstr(ty, val_ty);
        } else {
            /* struct, enum, or oneof value type */
            sstr_append_cstr(ty, sstr_cstr(f->map_value_type_name));
        }
        sstr_append_cstr(ty, ">");
    } else {
        const char* simple = rust_type_for(f->type);
        if (simple) {
            sstr_append_cstr(ty, simple);
        } else if (f->type == FIELD_TYPE_ENUM) {
            sstr_append_cstr(ty, sstr_cstr(f->type_name));
        } else if (f->type == FIELD_TYPE_STRUCT) {
            sstr_append_cstr(ty, sstr_cstr(f->type_name));
        } else if (f->type == FIELD_TYPE_ONEOF) {
            sstr_append_cstr(ty, sstr_cstr(f->type_name));
        } else {
            sstr_append_cstr(ty, sstr_cstr(f->type_name));
        }
    }

    /* Wrap in array / vec if needed */
    if (f->is_array) {
        sstr_t wrapped = sstr_new();
        if (f->array_size > 0) {
            /* fixed-size array: [T; N] */
            char buf[64];
            snprintf(buf, sizeof(buf), "[%s; %d]", sstr_cstr(ty), f->array_size);
            sstr_append_cstr(wrapped, buf);
        } else {
            sstr_append_cstr(wrapped, "Vec<");
            sstr_append(wrapped, ty);
            sstr_append_cstr(wrapped, ">");
        }
        sstr_free(ty);
        ty = wrapped;
    }

    (void)oneof_map;
    return ty;
}

/* ========================================================================= */
/* Default value helpers                                                     */
/* ========================================================================= */

/** Emit a Rust default value expression for a field. */
static void emit_default_expr(sstr_t out, struct struct_field* f) {
    const char* dv = sstr_cstr(f->default_value);
    if (f->type == FIELD_TYPE_BOOL) {
        sstr_append_cstr(out, dv);
    } else if (f->type == FIELD_TYPE_SSTR) {
        sstr_append_cstr(out, "String::from(\"");
        sstr_append_cstr(out, dv);
        sstr_append_cstr(out, "\")");
    } else if (f->type == FIELD_TYPE_ENUM) {
        /* Enum default: e.g. "GREEN" → Color::GREEN */
        sstr_append_cstr(out, sstr_cstr(f->type_name));
        sstr_append_cstr(out, "::");
        sstr_append_cstr(out, dv);
    } else if (f->type == FIELD_TYPE_FLOAT) {
        sstr_append_cstr(out, dv);
        sstr_append_cstr(out, "_f32");
    } else if (f->type == FIELD_TYPE_DOUBLE) {
        sstr_append_cstr(out, dv);
        sstr_append_cstr(out, "_f64");
    } else {
        /* integer types */
        sstr_append_cstr(out, dv);
    }
}

/* ========================================================================= */
/* Emit enums                                                                */
/* ========================================================================= */

struct enum_emit_ctx {
    sstr_t output;
};

static void emit_enum(void* key, void* value, void* ctx_ptr) {
    (void)key;
    struct enum_container* ec = (struct enum_container*)value;
    struct enum_emit_ctx* ctx = (struct enum_emit_ctx*)ctx_ptr;
    sstr_t out = ctx->output;

    sstr_append_cstr(out, "#[allow(non_camel_case_types)]\n");
    sstr_append_cstr(out, "#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]\n");
    sstr_append_cstr(out, "pub enum ");
    sstr_append(out, ec->name);
    sstr_append_cstr(out, " {\n");

    struct enum_value* ev = ec->values;
    while (ev) {
        if (ev->is_deprecated) {
            sstr_append_cstr(out, "    #[deprecated]\n");
        }
        sstr_append_cstr(out, "    ");
        sstr_append(out, ev->name);
        sstr_append_cstr(out, ",\n");
        ev = ev->next;
    }

    sstr_append_cstr(out, "}\n\n");
}

/* ========================================================================= */
/* Emit structs                                                              */
/* ========================================================================= */

struct rust_gen_ctx {
    sstr_t output;
    struct hash_map* enum_map;
    struct hash_map* oneof_map;
    struct hash_map* dep_map;  /* tracks emitted types for ordering */
};

static void emit_struct(struct struct_container* sc, struct rust_gen_ctx* ctx) {
    sstr_t out = ctx->output;
    int has_defaults = 0;

    sstr_append_cstr(out, "#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]\n");
    sstr_append_cstr(out, "pub struct ");
    sstr_append(out, sc->name);
    sstr_append_cstr(out, " {\n");

    struct struct_field* f = sc->fields;
    while (f) {
        int needs_option = f->is_optional || f->is_nullable;
        int needs_rename = (f->json_name && sstr_length(f->json_name) > 0);
        const char* jkey = json_key(f);

        /* Serde attributes */
        if (needs_rename || f->is_optional || f->has_default) {
            sstr_append_cstr(out, "    #[serde(");
            int first_attr = 1;
            if (needs_rename) {
                char buf[512];
                snprintf(buf, sizeof(buf), "rename = \"%s\"", jkey);
                sstr_append_cstr(out, buf);
                first_attr = 0;
            }
            if (f->is_optional) {
                if (!first_attr) sstr_append_cstr(out, ", ");
                sstr_append_cstr(out, "skip_serializing_if = \"Option::is_none\"");
                first_attr = 0;
            }
            if (needs_option) {
                if (!first_attr) sstr_append_cstr(out, ", ");
                sstr_append_cstr(out, "default");
                first_attr = 0;
            }
            if (f->has_default && !needs_option) {
                if (!first_attr) sstr_append_cstr(out, ", ");
                sstr_t fn_name = to_snake_case(sstr_cstr(sc->name));
                sstr_append_cstr(out, "default = \"default_");
                sstr_append(out, fn_name);
                sstr_append_cstr(out, "_");
                sstr_append(out, f->name);
                sstr_append_cstr(out, "\"");
                sstr_free(fn_name);
                first_attr = 0;
            }
            sstr_append_cstr(out, ")]\n");
        }

        if (f->is_deprecated) {
            sstr_append_cstr(out, "    #[deprecated]\n");
        }

        sstr_append_cstr(out, "    pub ");
        sstr_append(out, f->name);
        sstr_append_cstr(out, ": ");

        sstr_t ty = rust_field_type(f, ctx->oneof_map);
        if (needs_option) {
            sstr_append_cstr(out, "Option<");
            sstr_append(out, ty);
            sstr_append_cstr(out, ">");
        } else {
            sstr_append(out, ty);
        }
        sstr_free(ty);

        sstr_append_cstr(out, ",\n");

        if (f->has_default) has_defaults = 1;
        f = f->next;
    }

    sstr_append_cstr(out, "}\n\n");

    /* Generate Default impl if any field has a default value */
    if (has_defaults) {
        sstr_append_cstr(out, "impl Default for ");
        sstr_append(out, sc->name);
        sstr_append_cstr(out, " {\n");
        sstr_append_cstr(out, "    fn default() -> Self {\n");
        sstr_append_cstr(out, "        Self {\n");

        f = sc->fields;
        while (f) {
            sstr_append_cstr(out, "            ");
            sstr_append(out, f->name);
            sstr_append_cstr(out, ": ");

            if (f->is_optional || f->is_nullable) {
                if (f->has_default) {
                    sstr_append_cstr(out, "Some(");
                    emit_default_expr(out, f);
                    sstr_append_cstr(out, ")");
                } else {
                    sstr_append_cstr(out, "None");
                }
            } else if (f->has_default) {
                emit_default_expr(out, f);
            } else if (f->type == FIELD_TYPE_SSTR && !f->is_array) {
                sstr_append_cstr(out, "String::new()");
            } else if (f->type == FIELD_TYPE_BOOL && !f->is_array) {
                sstr_append_cstr(out, "false");
            } else if (is_numeric_type(f->type) && !f->is_array) {
                sstr_append_cstr(out, "0");
                /* Add type suffix for floats */
                if (f->type == FIELD_TYPE_FLOAT)
                    sstr_append_cstr(out, ".0_f32");
                else if (f->type == FIELD_TYPE_DOUBLE)
                    sstr_append_cstr(out, ".0_f64");
            } else if (f->is_array && f->array_size > 0) {
                sstr_append_cstr(out, "Default::default()");
            } else if (f->is_array) {
                sstr_append_cstr(out, "Vec::new()");
            } else if (f->type == FIELD_TYPE_MAP) {
                if (f->is_array) {
                    sstr_append_cstr(out, "Vec::new()");
                } else {
                    sstr_append_cstr(out, "HashMap::new()");
                }
            } else {
                /* struct/enum/oneof — use Default trait */
                sstr_append_cstr(out, "Default::default()");
            }

            sstr_append_cstr(out, ",\n");
            f = f->next;
        }

        sstr_append_cstr(out, "        }\n");
        sstr_append_cstr(out, "    }\n");
        sstr_append_cstr(out, "}\n\n");

        /* Emit standalone default functions for serde */
        f = sc->fields;
        while (f) {
            if (f->has_default && !f->is_optional && !f->is_nullable) {
                sstr_t fn_name = to_snake_case(sstr_cstr(sc->name));
                sstr_append_cstr(out, "#[allow(non_snake_case)]\n");
                sstr_append_cstr(out, "fn default_");
                sstr_append(out, fn_name);
                sstr_append_cstr(out, "_");
                sstr_append(out, f->name);
                sstr_append_cstr(out, "() -> ");
                sstr_t ty = rust_field_type(f, ctx->oneof_map);
                sstr_append(out, ty);
                sstr_free(ty);
                sstr_append_cstr(out, " { ");
                emit_default_expr(out, f);
                sstr_append_cstr(out, " }\n\n");
                sstr_free(fn_name);
            }
            f = f->next;
        }
    }
}

/* ========================================================================= */
/* Emit oneofs (tagged unions)                                               */
/* ========================================================================= */

static void emit_oneof(struct oneof_container* oc, struct rust_gen_ctx* ctx) {
    sstr_t out = ctx->output;

    sstr_append_cstr(out, "#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]\n");

    /* Use serde's internally-tagged representation */
    sstr_append_cstr(out, "#[serde(tag = \"");
    sstr_append(out, oc->tag_field);
    sstr_append_cstr(out, "\")]\n");

    sstr_append_cstr(out, "pub enum ");
    sstr_append(out, oc->name);
    sstr_append_cstr(out, " {\n");

    struct oneof_variant* v = oc->variants;
    while (v) {
        if (v->is_deprecated) {
            sstr_append_cstr(out, "    #[deprecated]\n");
        }
        /* Rename to the variant name used as the tag value in JSON */
        sstr_append_cstr(out, "    #[serde(rename = \"");
        sstr_append(out, v->name);
        sstr_append_cstr(out, "\")]\n");

        /* Use PascalCase for the Rust variant name (capitalize first letter) */
        char first = (char)toupper(sstr_cstr(v->name)[0]);
        sstr_append_cstr(out, "    ");
        sstr_append_of(out, &first, 1);
        sstr_append_cstr(out, sstr_cstr(v->name) + 1);

        /* Flatten the struct fields into the variant */
        sstr_append_cstr(out, "(");
        sstr_append(out, v->struct_type_name);
        sstr_append_cstr(out, "),\n");

        v = v->next;
    }

    sstr_append_cstr(out, "}\n\n");
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

/** Check if all dependencies of a struct are emitted. */
static int struct_deps_ready(struct struct_container* sc,
                             struct hash_map* dep_map) {
    struct struct_field* f = sc->fields;
    while (f) {
        if (f->type == FIELD_TYPE_STRUCT || f->type == FIELD_TYPE_ONEOF) {
            if (!is_emitted(dep_map, sstr_cstr(f->type_name)))
                return 0;
        }
        if (f->type == FIELD_TYPE_ENUM) {
            /* enums are emitted first, always ready */
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

/** Check if all variant types of a oneof are emitted. */
static int oneof_deps_ready(struct oneof_container* oc, struct hash_map* dep_map) {
    struct oneof_variant* v = oc->variants;
    while (v) {
        if (!is_emitted(dep_map, sstr_cstr(v->struct_type_name)))
            return 0;
        v = v->next;
    }
    return 1;
}

struct dep_emit_struct_ctx {
    struct rust_gen_ctx* gen;
    int emitted_any;
};

static void try_emit_struct(void* key, void* value, void* ctx_ptr) {
    sstr_t k = (sstr_t)key;
    struct struct_container* sc = (struct struct_container*)value;
    struct dep_emit_struct_ctx* ctx = (struct dep_emit_struct_ctx*)ctx_ptr;

    if (is_emitted(ctx->gen->dep_map, sstr_cstr(k))) return;
    if (!struct_deps_ready(sc, ctx->gen->dep_map)) return;

    emit_struct(sc, ctx->gen);
    mark_emitted(ctx->gen->dep_map, sstr_cstr(k));
    ctx->emitted_any = 1;
}

static void try_emit_oneof(void* key, void* value, void* ctx_ptr) {
    sstr_t k = (sstr_t)key;
    struct oneof_container* oc = (struct oneof_container*)value;
    struct dep_emit_struct_ctx* ctx = (struct dep_emit_struct_ctx*)ctx_ptr;

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

int gencode_rust(struct hash_map* struct_map, struct hash_map* enum_map,
                 struct hash_map* oneof_map, sstr_t output) {
    struct hash_map* dep_map =
        hash_map_new(256, sstr_key_hash, sstr_key_cmp, sstr_key_free,
                     dummy_free);
    if (dep_map == NULL) return -1;

    struct rust_gen_ctx ctx;
    ctx.output = output;
    ctx.enum_map = enum_map;
    ctx.oneof_map = oneof_map;
    ctx.dep_map = dep_map;

    /* File header */
    sstr_append_cstr(output,
        "// Auto-generated Rust module — DO NOT EDIT.\n"
        "// Generated by json-gen-c with --rust.\n\n"
        "#![allow(dead_code, non_snake_case, non_camel_case_types)]\n\n"
        "use serde::{Deserialize, Serialize};\n"
        "use std::collections::HashMap;\n\n");

    /* 1) Emit all enums first (no dependencies) */
    struct enum_emit_ctx ectx;
    ectx.output = output;
    hash_map_for_each(enum_map, emit_enum, &ectx);

    /* Mark enums as emitted in the dependency map */
    hash_map_for_each(enum_map, mark_enum_emitted, dep_map);

    /* 2) Emit structs and oneofs in dependency order */
    int total = struct_map->size + oneof_map->size;
    for (int round = 0; round < total; ++round) {
        struct dep_emit_struct_ctx dctx;
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
        fprintf(stderr, "Error: could not resolve all type dependencies for Rust generation\n");
        return -1;
    }

    return 0;
}
