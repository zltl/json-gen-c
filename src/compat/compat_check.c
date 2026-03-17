/**
 * @file compat/compat_check.c
 * @brief Schema compatibility checker implementation.
 */

#include "compat/compat_check.h"

#include <stdio.h>
#include <string.h>

#include "struct/struct_parse.h"
#include "utils/sstr.h"

/* ---- helpers ---- */

static const char *field_type_str(int type) {
    switch (type) {
    case FIELD_TYPE_INT:    return "int";
    case FIELD_TYPE_LONG:   return "long";
    case FIELD_TYPE_FLOAT:  return "float";
    case FIELD_TYPE_DOUBLE: return "double";
    case FIELD_TYPE_SSTR:   return "sstr_t";
    case FIELD_TYPE_ENUM:   return "enum";
    case FIELD_TYPE_STRUCT: return "struct";
    case FIELD_TYPE_BOOL:   return "bool";
    case FIELD_TYPE_MAP:    return "map";
    case FIELD_TYPE_INT8:   return "int8_t";
    case FIELD_TYPE_INT16:  return "int16_t";
    case FIELD_TYPE_INT32:  return "int32_t";
    case FIELD_TYPE_INT64:  return "int64_t";
    case FIELD_TYPE_UINT8:  return "uint8_t";
    case FIELD_TYPE_UINT16: return "uint16_t";
    case FIELD_TYPE_UINT32: return "uint32_t";
    case FIELD_TYPE_UINT64: return "uint64_t";
    case FIELD_TYPE_ONEOF:  return "oneof";
    default:                return "?";
    }
}

/* ---- per-struct comparison ---- */

struct compat_ctx {
    int breaking;
    int safe;
};

static void check_struct_pair(struct struct_container *old_sc,
                              struct struct_container *new_sc,
                              struct compat_ctx *ctx) {
    /* Check removed fields (in old but not in new). */
    for (struct struct_field *of = old_sc->fields; of; of = of->next) {
        int found = 0;
        for (struct struct_field *nf = new_sc->fields; nf; nf = nf->next) {
            if (sstr_compare(of->name, nf->name) == 0) {
                found = 1;
                /* Check type change. */
                if (of->type != nf->type ||
                    of->is_array != nf->is_array ||
                    of->array_size != nf->array_size) {
                    printf("  BREAKING: field '%s' in struct '%s' changed type "
                           "(%s%s -> %s%s)\n",
                           sstr_cstr(of->name), sstr_cstr(old_sc->name),
                           field_type_str(of->type),
                           of->is_array ? "[]" : "",
                           field_type_str(nf->type),
                           nf->is_array ? "[]" : "");
                    ctx->breaking++;
                }
                /* Check newly deprecated. */
                if (!of->is_deprecated && nf->is_deprecated) {
                    printf("  info: field '%s' in struct '%s' is now "
                           "@deprecated\n",
                           sstr_cstr(of->name), sstr_cstr(old_sc->name));
                    ctx->safe++;
                }
                break;
            }
        }
        if (!found) {
            printf("  BREAKING: field '%s' removed from struct '%s'\n",
                   sstr_cstr(of->name), sstr_cstr(old_sc->name));
            ctx->breaking++;
        }
    }
    /* Check added fields (in new but not in old). */
    for (struct struct_field *nf = new_sc->fields; nf; nf = nf->next) {
        int found = 0;
        for (struct struct_field *of = old_sc->fields; of; of = of->next) {
            if (sstr_compare(nf->name, of->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (nf->is_optional) {
                printf("  safe: optional field '%s' added to struct '%s'\n",
                       sstr_cstr(nf->name), sstr_cstr(new_sc->name));
            } else {
                printf("  safe: field '%s' added to struct '%s'\n",
                       sstr_cstr(nf->name), sstr_cstr(new_sc->name));
            }
            ctx->safe++;
        }
    }
}

/* ---- per-enum comparison ---- */

static void check_enum_pair(struct enum_container *old_ec,
                            struct enum_container *new_ec,
                            struct compat_ctx *ctx) {
    /* Check removed values. */
    for (struct enum_value *ov = old_ec->values; ov; ov = ov->next) {
        int found = 0;
        for (struct enum_value *nv = new_ec->values; nv; nv = nv->next) {
            if (sstr_compare(ov->name, nv->name) == 0) {
                found = 1;
                if (!ov->is_deprecated && nv->is_deprecated) {
                    printf("  info: enum value '%s' in '%s' is now "
                           "@deprecated\n",
                           sstr_cstr(ov->name), sstr_cstr(old_ec->name));
                    ctx->safe++;
                }
                break;
            }
        }
        if (!found) {
            printf("  BREAKING: enum value '%s' removed from '%s'\n",
                   sstr_cstr(ov->name), sstr_cstr(old_ec->name));
            ctx->breaking++;
        }
    }
    /* Check added values. */
    for (struct enum_value *nv = new_ec->values; nv; nv = nv->next) {
        int found = 0;
        for (struct enum_value *ov = old_ec->values; ov; ov = ov->next) {
            if (sstr_compare(nv->name, ov->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("  safe: enum value '%s' added to '%s'\n",
                   sstr_cstr(nv->name), sstr_cstr(new_ec->name));
            ctx->safe++;
        }
    }
}

/* ---- per-oneof comparison ---- */

static void check_oneof_pair(struct oneof_container *old_oc,
                             struct oneof_container *new_oc,
                             struct compat_ctx *ctx) {
    /* Check removed variants. */
    for (struct oneof_variant *ov = old_oc->variants; ov; ov = ov->next) {
        int found = 0;
        for (struct oneof_variant *nv = new_oc->variants; nv; nv = nv->next) {
            if (sstr_compare(ov->name, nv->name) == 0) {
                found = 1;
                if (!ov->is_deprecated && nv->is_deprecated) {
                    printf("  info: oneof variant '%s' in '%s' is now "
                           "@deprecated\n",
                           sstr_cstr(ov->name), sstr_cstr(old_oc->name));
                    ctx->safe++;
                }
                break;
            }
        }
        if (!found) {
            printf("  BREAKING: oneof variant '%s' removed from '%s'\n",
                   sstr_cstr(ov->name), sstr_cstr(old_oc->name));
            ctx->breaking++;
        }
    }
    /* Check added variants. */
    for (struct oneof_variant *nv = new_oc->variants; nv; nv = nv->next) {
        int found = 0;
        for (struct oneof_variant *ov = old_oc->variants; ov; ov = ov->next) {
            if (sstr_compare(nv->name, ov->name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("  safe: oneof variant '%s' added to '%s'\n",
                   sstr_cstr(nv->name), sstr_cstr(new_oc->name));
            ctx->safe++;
        }
    }
}

/* ---- hash_map_for_each callbacks ---- */

struct check_pair_args {
    struct hash_map *new_map;
    struct compat_ctx *ctx;
};

/* Called for each struct in the OLD schema. */
static void check_struct_cb(void *key, void *value, void *arg) {
    (void)key;
    struct struct_container *old_sc = (struct struct_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *new_val = NULL;
    if (hash_map_find(a->new_map, old_sc->name, &new_val) == HASH_MAP_OK) {
        check_struct_pair(old_sc, (struct struct_container *)new_val,
                          a->ctx);
    } else {
        printf("  BREAKING: struct '%s' removed\n", sstr_cstr(old_sc->name));
        a->ctx->breaking++;
    }
}

/* Called for each struct in the NEW schema (to find additions). */
static void check_struct_added_cb(void *key, void *value, void *arg) {
    (void)key;
    struct struct_container *new_sc = (struct struct_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *old_val = NULL;
    /* new_map here is actually old_map (role swapped by caller). */
    if (hash_map_find(a->new_map, new_sc->name, &old_val) != HASH_MAP_OK) {
        printf("  safe: struct '%s' added\n", sstr_cstr(new_sc->name));
        a->ctx->safe++;
    }
}

static void check_enum_cb(void *key, void *value, void *arg) {
    (void)key;
    struct enum_container *old_ec = (struct enum_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *new_val = NULL;
    if (hash_map_find(a->new_map, old_ec->name, &new_val) == HASH_MAP_OK) {
        check_enum_pair(old_ec, (struct enum_container *)new_val, a->ctx);
    } else {
        printf("  BREAKING: enum '%s' removed\n", sstr_cstr(old_ec->name));
        a->ctx->breaking++;
    }
}

static void check_enum_added_cb(void *key, void *value, void *arg) {
    (void)key;
    struct enum_container *new_ec = (struct enum_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *old_val = NULL;
    if (hash_map_find(a->new_map, new_ec->name, &old_val) != HASH_MAP_OK) {
        printf("  safe: enum '%s' added\n", sstr_cstr(new_ec->name));
        a->ctx->safe++;
    }
}

static void check_oneof_cb(void *key, void *value, void *arg) {
    (void)key;
    struct oneof_container *old_oc = (struct oneof_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *new_val = NULL;
    if (hash_map_find(a->new_map, old_oc->name, &new_val) == HASH_MAP_OK) {
        check_oneof_pair(old_oc, (struct oneof_container *)new_val, a->ctx);
    } else {
        printf("  BREAKING: oneof '%s' removed\n", sstr_cstr(old_oc->name));
        a->ctx->breaking++;
    }
}

static void check_oneof_added_cb(void *key, void *value, void *arg) {
    (void)key;
    struct oneof_container *new_oc = (struct oneof_container *)value;
    struct check_pair_args *a = (struct check_pair_args *)arg;
    void *old_val = NULL;
    if (hash_map_find(a->new_map, new_oc->name, &old_val) != HASH_MAP_OK) {
        printf("  safe: oneof '%s' added\n", sstr_cstr(new_oc->name));
        a->ctx->safe++;
    }
}

/* ---- public API ---- */

int compat_check(struct hash_map *old_structs, struct hash_map *old_enums,
                 struct hash_map *old_oneofs, struct hash_map *new_structs,
                 struct hash_map *new_enums, struct hash_map *new_oneofs) {
    struct compat_ctx ctx = {0, 0};
    struct check_pair_args args;

    /* Structs: removals + field changes. */
    args.new_map = new_structs;
    args.ctx = &ctx;
    hash_map_for_each(old_structs, check_struct_cb, &args);

    /* Structs: additions. */
    args.new_map = old_structs;
    hash_map_for_each(new_structs, check_struct_added_cb, &args);

    /* Enums: removals + value changes. */
    args.new_map = new_enums;
    hash_map_for_each(old_enums, check_enum_cb, &args);

    /* Enums: additions. */
    args.new_map = old_enums;
    hash_map_for_each(new_enums, check_enum_added_cb, &args);

    /* Oneofs: removals + variant changes. */
    args.new_map = new_oneofs;
    hash_map_for_each(old_oneofs, check_oneof_cb, &args);

    /* Oneofs: additions. */
    args.new_map = old_oneofs;
    hash_map_for_each(new_oneofs, check_oneof_added_cb, &args);

    /* Summary. */
    if (ctx.breaking == 0 && ctx.safe == 0) {
        printf("Schemas are identical.\n");
        return 0;
    }
    printf("\nSummary: %d breaking change(s), %d safe change(s).\n",
           ctx.breaking, ctx.safe);
    return ctx.breaking > 0 ? 1 : 0;
}
