# Schema Evolution Guide

This document describes how to safely evolve `.json-gen-c` schemas over time without breaking existing serialized data or downstream consumers.

## Overview

As applications grow, their data schemas inevitably change: new fields are added, old fields become irrelevant, and types may need adjustment. Schema evolution is the practice of making these changes in a controlled way so that **old data can still be read by new code** (forward compatibility) and **new data can still be read by old code** (backward compatibility).

## Forward Compatibility (Already Built-In)

json-gen-c's generated unmarshal functions **silently skip unknown JSON keys**. This means:

- If you add a new field to a schema and regenerate, old JSON data (which lacks that field) can still be parsed. The new field will retain its `_init()` default value.
- No special annotations are needed — this works automatically.

## Backward Compatibility

When new code produces JSON with new fields, old code (generated from an earlier schema version) simply ignores the unknown keys. This is also automatic.

**Caveat:** if old code re-marshals the struct, the unknown fields will be lost (they were never stored). This is acceptable for most use cases but important to understand for data pipeline scenarios.

## Safe Changes

These changes are **backward- and forward-compatible** — they will not break existing consumers:

| Change | Why It's Safe |
|--------|---------------|
| **Add a new field** | Old data is parsed normally; the new field gets its init default. Old code ignores the new key. |
| **Add a new `optional` field** | Same as above, plus `has_<field>` will be `false` for old data. |
| **Add a new `nullable` field** | Same as above. |
| **Add a new enum value** | Old code that encounters the unknown string will fail unmarshal (see caveats). |
| **Add a new oneof variant** | Old code ignores the unknown tag value. |
| **Mark a field `@deprecated`** | No wire-format change. Downstream C code gets compiler deprecation warnings. |
| **Add a default value to a field** | Only affects `_init()`; no wire-format change. |
| **Rename a field's JSON key with `@json`** | If the old JSON key remains via alias, this is safe. |

## Breaking Changes

These changes **will break** existing consumers and should be avoided or carefully managed:

| Change | Why It Breaks |
|--------|---------------|
| **Remove a field** | Old JSON containing the removed key is silently ignored (safe for unmarshal), but code that accesses the field will fail to compile. |
| **Change a field's type** | Unmarshal will produce incorrect values or fail. |
| **Rename a field without `@json` alias** | Old JSON with the previous key is ignored; the renamed field gets its default. |
| **Remove an enum value** | Code referencing the removed constant fails to compile. Old JSON with the removed string fails unmarshal. |
| **Remove a oneof variant** | Code referencing the removed variant fails to compile. |
| **Change a field from non-array to array (or vice versa)** | Wire format mismatch. |
| **Change array from fixed to dynamic (or vice versa)** | Generated struct layout changes. |

## Migration Patterns

### Renaming a Field

Use `@json` to keep the old wire-format name while changing the C identifier:

```
struct User {
    @json "name" sstr_t display_name;  // was: sstr_t name
};
```

Old JSON `{"name": "Alice"}` still unmarshals correctly into `display_name`.

### Soft-Removing a Field

Mark it `@deprecated` instead of deleting it:

```
struct Config {
    int timeout_ms;
    @deprecated int timeout_sec;  // use timeout_ms instead
};
```

The field remains fully functional in marshal/unmarshal, but any C code accessing `obj->timeout_sec` will trigger a compiler deprecation warning.

### Adding a Required Field to Existing Data

Make the new field `optional` so that old data (which lacks it) still parses:

```
struct Event {
    sstr_t name;
    optional sstr_t correlation_id;  // added in v2
};
```

Check `obj->has_correlation_id` before using the field.

### Widening a Numeric Type

Changing `int` to `long` or `float` to `double` changes the generated struct layout. Instead, add a new field and deprecate the old one:

```
struct Measurement {
    @deprecated int value;       // old: 32-bit
    optional long value_v2;      // new: 64-bit
};
```

Application code can check `has_value_v2` and fall back to `value` for old data.

## The `@deprecated` Annotation

### Syntax

`@deprecated` can be placed before a field declaration, enum value, or oneof variant:

```
struct Foo {
    @deprecated int old_field;
    @deprecated @json "old_name" sstr_t legacy_name;
    @deprecated optional int removed_soon;
};

enum Status {
    ACTIVE,
    @deprecated INACTIVE,
    ARCHIVED
};

oneof Shape {
    Circle c;
    @deprecated Square sq;
};
```

### Behavior

- **Marshal/unmarshal:** Deprecated fields, enum values, and oneof variants are **fully functional**. No runtime behavior changes.
- **Generated C code:** Struct members for deprecated fields are annotated with compiler-specific deprecation attributes:
  - GCC/Clang: `__attribute__((deprecated("message")))`
  - MSVC: `__declspec(deprecated("message"))`
- **Enum constants:** Deprecated enum values use `__attribute__((deprecated))` on GCC/Clang. MSVC does not support per-enumerator deprecation.
- **Effect:** Any C code that directly accesses a deprecated struct member or enum constant will produce a compiler warning, alerting developers to migrate away from it.

### Generated Code Example

For a schema with `@deprecated int old_count;`, the generated header contains:

```c
struct Foo {
    int active;
    JGENC_DEPRECATED("old_count is deprecated") int old_count;
};
```

The `JGENC_DEPRECATED` macro is defined in the generated header and can be overridden by defining it before including the generated header.

## Best Practices

1. **Always add new fields as `optional`** when backward compatibility matters. This ensures old data without the field still parses correctly.

2. **Use `@json` aliases when renaming** to maintain wire-format compatibility.

3. **Deprecate before removing.** Mark fields `@deprecated` in one release, then remove them in a later release after consumers have migrated.

4. **Never change a field's type in-place.** Instead, add a new field with the desired type and deprecate the old one.

5. **Document field lifecycle** in comments next to `@deprecated` annotations to help future maintainers understand when and why a field was deprecated.

6. **Use `--check-compat`** to automatically detect breaking changes between schema versions before deploying.

## See Also

- [README.md](../README.md) — project overview and API reference
- [IMPROVEMENTS.md](../docs/IMPROVEMENTS.md) — project roadmap
