# Project Improvement Plan

This document tracks the current maturity of `json-gen-c`, completed foundational work, and the long-term roadmap for making it a more complete production-grade schema-first JSON code generator for C.

## Current Assessment

| Area | Status | Notes |
|------|--------|-------|
| Core functionality | Good | Supports `int`, `long`, `float`, `double`, `bool`, `sstr_t`, arrays, nested structs, and `#include` across schema files |
| Test coverage | Good | Multiple Google Test suites, benchmark coverage, `-Werror`, bugfix regression tests |
| Documentation | Good | README, getting started guide, Doxygen, and man page exist |
| CI/CD | Good | GitHub Actions test workflow and Doxygen deployment are present |
| Type system | Growing | Enums, fixed-size arrays, maps, optional/nullable fields, and precise-width integers supported |
| Ecosystem integration | Limited | No CMake, pkg-config, package manager distribution, or Windows support |

Summary: the project is already usable and reasonably mature at the core. Enum type support has been added, expanding the type system. It still needs broader type support (tagged unions), better ecosystem integration, and stronger developer ergonomics to reach a polished 1.0.

## Completed Foundational Work

### Implement Hash Map Resizing
**Location:** `src/utils/hash_map.c`

Implemented resizing logic in `hash_map_resize_if_needed()`: allocate a larger bucket array, rehash existing entries, free the old buckets, and update map metadata. Resizing is triggered when the load factor exceeds 0.75.

**Status:** Completed.

### Optimize `hash_2s`
**Location:** `src/gencode/gencode.c`

Removed temporary string concatenation from the composite hash path. The implementation now chains `hash_murmur()` calls with a seed, avoiding per-call allocation overhead.

**Status:** Completed.

### Unify Hash Functions
**Location:** `src/utils/hash.h`, `src/utils/hash.c`

Consolidated the MurmurHash-like implementation into a shared utility function, `hash_murmur()`, and reused it across code generation and hash map logic.

**Status:** Completed.

### Use `stdbool.h`
**Location:** `src/utils/sstr.c`, `src/utils/sstr.h`, related files

Refactored boolean-like integer flags to use `bool` where appropriate for clarity and consistency.

**Status:** Completed.

### Improve Argument Parsing
**Location:** `src/main/main.c`

Replaced brittle manual argument parsing with `getopt_long_only`, improving flag handling and future extensibility.

**Status:** Completed.

### Improve Error Handling Consistency
**Location:** `src/main/main.c`

Adjusted `usage()` handling so error-triggered output goes to `stderr` while normal help output goes to `stdout`.

**Status:** Completed.

### Fix Parser Off-by-One Bug
**Location:** `src/struct/struct_parse.c`

Fixed the parser EOF condition from `offset >= length - 1` to `offset >= length`, preventing the last significant character of a schema file from being ignored.

**Status:** Completed.

### Refactor Top-Level `#include` Parsing
**Location:** `src/struct/struct_parse.c`

Restructured top-level parsing so `#include` directives and `struct` declarations are handled independently in `struct_parser_parse()`. This fixes failures when `#include` appears before struct definitions in the same schema file.

**Status:** Completed.

### Harden File I/O
**Location:** `src/utils/io.c`

Added proper error checking for `fopen`, `fseek`, `ftell`, `fread`, and `fwrite`.

**Status:** Completed.

### Add Enum Type Support
**Location:** `src/struct/struct_parse.h`, `src/struct/struct_parse.c`, `src/gencode/gencode.h`, `src/gencode/gencode.c`, `src/gencode/codes/json_parse.h`, `src/gencode/codes/json_parse.c`, `src/main/main.c`

Implemented full enum type support across the schema parser, code generator, and generated runtime:

- **Schema syntax:** `enum Color { RED, GREEN, BLUE }` — comma or semicolon separated values.
- **Parser:** Added `enum_map` to `struct_parser`, `parse_enum_body()` parsing function, and `TOKEN_COMMA` tokenizer support. Enum maps are shared across `#include` sub-parsers.
- **Field recognition:** When a field type matches a known enum name, it is marked `FIELD_TYPE_ENUM` instead of `FIELD_TYPE_STRUCT`.
- **Header generation:** Generates C `enum` type definitions with `EnumName_VALUE = N` constants in the output `.h` file.
- **Source generation:** Generates `static const char* EnumName_enum_strings[]` and `static const int EnumName_enum_count` arrays for string lookup.
- **Marshal:** Scalar enum fields output as JSON strings via string table lookup. Out-of-range values fall back to integer output. Enum arrays marshal each element the same way.
- **Unmarshal:** Accepts both JSON strings (matched against string table) and JSON integers. Unknown string values produce an error. Array unmarshal uses the same logic per element.
- **Offset map:** Extended `json_field_offset_item` with `enum_strings` and `enum_count` fields so the runtime can access per-field enum metadata.
- **Storage:** Enum fields are stored as `int` in generated C structs.

**Test coverage:** 11 dedicated enum tests covering scalar marshal/unmarshal, array marshal/unmarshal, round-trip, integer fallback, unknown value errors, out-of-range marshal, generated constants verification, and empty array. Empty enum regression test added to bugfix suite.

**Status:** Completed.

### Add Fixed-Size Array Support
**Location:** `src/struct/struct_parse.h`, `src/struct/struct_parse.c`, `src/gencode/gencode.c`, `src/gencode/codes/json_parse.c`

Implemented fixed-size array support, distinguished from existing dynamically allocated arrays:

- **Schema syntax:** `int data[10];` — any supported type followed by `[N]` where N is a positive integer.
- **Parser:** Added `array_size` field to `struct_field`. When `[N]` is parsed, `is_array` is set and `array_size` stores N (>0 means fixed-size; 0 means dynamic).
- **Header generation:** Fixed-size arrays emit inline C arrays (`type name[N];`) with no pointer or `_len` companion field.
- **Init generation:** Primitive fixed arrays use `memset()` to zero; `sstr_t` and struct arrays loop to initialize each element.
- **Clear generation:** Primitive fixed arrays use `memset()` to zero; `sstr_t` arrays loop to `sstr_free()` and NULL each element; struct arrays loop to call the nested clear function.
- **Marshal generation:** Fixed arrays use the compile-time size N as loop bound instead of a runtime `_len` field.
- **Offset/metadata table:** Extended `json_field_offset_item` with `array_size` field. Only dynamic arrays generate the extra `_len` metadata entry.
- **Unmarshal (runtime):** New dispatch block for `fi->is_array && fi->array_size > 0` parses JSON array elements directly into the inline buffer, enforcing the maximum element count and reporting an error on overflow.

**Test coverage:** 17 dedicated tests covering init zeros, clear resets, marshal (int array, zero-init), round-trip for all supported types (int, long, float, double, string, bool, enum, struct), partial fill unmarshal, empty array unmarshal, overflow error detection, struct layout verification, and full round-trip with all field types.

**Status:** Completed.

### Add Map/Dictionary Support
**Location:** `src/struct/struct_parse.h`, `src/struct/struct_parse.c`, `src/gencode/gencode.c`, `src/gencode/codes/json_parse.h`, `src/gencode/codes/json_parse.c`

Implemented map/dictionary field support that marshals to/from JSON objects:

- **Schema syntax:** `map<sstr_t, V> field;` for scalar maps, `map<sstr_t, V> field[];` for arrays of maps. Supported value types: `int`, `long`, `float`, `double`, `bool`, `sstr_t`, enum, and struct.
- **Key type:** Always `sstr_t` (JSON object keys are strings).
- **C representation:** Each map value type generates a pair of structs via `#ifndef` guards to avoid duplication:
  - `struct json_map_entry_<V> { sstr_t key; V value; };`
  - `struct json_map_<V> { struct json_map_entry_<V>* entries; int len; };`
  - Bool and enum value types share `json_map_int` since both are stored as `int`.
- **Parser:** Detects `map` keyword, reads `<key_type, value_type>` via the existing angle-bracket tokenizer, validates key is `sstr_t`, and determines value type (builtin, enum, or struct). Also fixed a pre-existing tokenizer bug where the `<...>` string parser did not advance the offset correctly past the closing `>`.
- **Init:** Scalar maps initialize `entries = NULL, len = 0`. Array-of-maps initializes the pointer to NULL and `_len` to 0.
- **Clear:** Frees each entry's key, clears sstr_t/struct values, frees the entries array. Array-of-maps adds an outer loop over each map container.
- **Marshal:** Scalar maps output `{"key1":val1,"key2":val2}`. Array-of-maps outputs `[{...},{...}]`. Enum values marshal as strings via string table lookup.
- **Unmarshal (runtime):** New `json_unmarshal_map_object()` function parses a JSON `{...}` into a map container, dynamically growing the entries array with realloc. Array-of-maps parses `[{...},{...}]` by growing the outer array. Uses `map_value_type` and `map_entry_size` from the offset table for type-safe dispatch.
- **Offset table:** Extended `json_field_offset_item` with `map_value_type` and `map_entry_size` fields. Enum map values include `enum_strings`/`enum_count` references for runtime string lookup.

**Test coverage:** 10 dedicated tests covering init/clear, empty map marshal, int map marshal, int map unmarshal, round-trip, empty map unmarshal, all-types round-trip (int, long, float, double, bool, sstr_t, enum, struct), array-of-maps round-trip, empty array-of-maps, and null map handling.

**Status:** Completed.

## Roadmap

### Phase 1: Bug Fixes and Technical Debt Cleanup

1. Keep parser regression coverage growing for edge cases around comments, includes, empty files, and malformed trailing tokens.
2. Continue refactoring parser internals so responsibilities are clearer and easier to extend.
3. Remove small remaining inconsistencies in diagnostics, naming, and internal APIs.
4. Audit the generated code template for duplicated logic and simplify where possible.

**Exit criteria:** CI stays green, regression tests cover recent parser and generator fixes, and the parsing/codegen core is stable enough for feature work.

### Phase 2: Type System Expansion

1. Add enum support.
    - ~~Example: `enum Color { RED, GREEN, BLUE }`~~
    - ~~Default JSON representation should be string-based, with room for future annotations for integer representation.~~
    - **Completed.** Enums are supported with string-based JSON representation. Integer unmarshal is also accepted. See "Add Enum Type Support" in the completed section above.
2. ~~Add fixed-size arrays.~~
    - ~~Example: `int data[10];`~~
    - ~~Distinguish them from dynamically allocated arrays.~~
    - **Completed.** Fixed-size arrays are supported with `type name[N];` syntax. See "Add Fixed-Size Array Support" in the completed section above.
3. ~~Add map or dictionary-like support.~~
    - ~~Example direction: `map<sstr_t, int>`~~
    - ~~Map naturally to JSON objects.~~
    - **Completed.** Map/dictionary fields are supported with `map<sstr_t, V>` syntax. See "Add Map/Dictionary Support" in the completed section above.
4. ~~Add optional or nullable fields.~~
    - ~~Missing fields during unmarshal should not always be treated as hard errors.~~
    - **Completed.** Two independent prefix keywords `optional` and `nullable` can be used individually or combined.
      - `optional int x;` — field may be absent; marshal skips when `has_x == false`; unmarshal sets `has_x = true` on presence.
      - `nullable sstr_t y;` — field always appears in JSON; marshal emits `"y":null` when `has_y == false`; unmarshal accepts JSON `null`.
      - `optional nullable int z;` — combined: marshal skips when absent; unmarshal accepts both missing and null.
      - Each generates a `bool has_<field>;` companion in the C struct. Non-decorated fields keep exact backward-compatible behavior.
5. ~~Add precise-width integer families.~~
    - ~~`int8`, `int16`, `uint16`, `uint32`, `uint64`, `int64`, and related forms.~~
    - **Completed.** All 8 standard C precise-width integer types are supported as first-class types:
      `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`.
      - Usable as scalar fields, fixed-size arrays, dynamic arrays, and map values.
      - Marshal uses efficient sstr_append helpers; unmarshal performs strict range checking.
      - Generated header includes `<stdint.h>` automatically.
6. Evaluate a tagged union or `oneof` style feature as a longer-term extension.

**Exit criteria:** each new type has parser coverage, generator coverage, runtime coverage, and example schemas.

### Phase 3: Developer Experience

1. Improve diagnostics.
    - Show filename, line, column, and a short source snippet in a clang-like format.
2. Add schema validation before code generation.
    - Undefined type references
    - Duplicate field names
    - Duplicate struct names
    - Invalid naming patterns
3. Add JSON field alias support.
    - Example direction: annotation-style comments such as `@json:"field_name"`
4. Add default value support.
    - Example direction: `int count = 10;`
    - Missing JSON fields can initialize to declared defaults.
5. Add `--version` output and centralize version definition in the build and source tree.
6. Update the man page to match the current feature set and CLI behavior.

**Exit criteria:** the tool is easier to debug, easier to learn, and easier to integrate into scripted workflows.

### Phase 4: Build Ecosystem and Cross-Platform Support

1. Add CMake support.
    - Enable `find_package` or easy `add_custom_command` integration.
2. Add pkg-config metadata.
3. Add package-manager-friendly distribution paths.
    - Homebrew
    - Debian packaging
    - AUR or equivalent community packaging
4. Add Windows support.
    - Validate GCC/Clang on Windows
    - Validate MSVC compatibility where practical
5. Consider Meson as an additional build integration option.

**Exit criteria:** building and consuming the tool becomes straightforward on Linux, macOS, and Windows, with standard project integration paths.

### Phase 5: Performance and Reliability

1. Add custom allocator support.
    - Allow generated code to use caller-provided allocation and free hooks.
    - Important for embedded targets and arena allocators.
2. Add fuzz testing.
    - Schema parser fuzzing
    - JSON parser fuzzing
3. Add memory tooling in CI.
    - AddressSanitizer
    - Leak checking
    - Optional Valgrind runs where practical
4. Build comparative benchmarks against similar JSON libraries and code-generation approaches.
5. Investigate selective parsing or SIMD-assisted fast paths only after correctness and coverage are strong.

**Exit criteria:** the tool has stronger crash resistance, clearer performance baselines, and better support for constrained environments.

### Phase 6: Long-Term Vision

1. Add schema evolution support.
    - Forward and backward compatibility rules
    - Field lifecycle guidance
2. Consider additional generated serialization formats.
    - MessagePack
    - CBOR
3. Explore multi-language bindings or code generation targets.
    - C++
    - Rust
    - Go
4. Build better authoring tools.
    - VS Code syntax highlighting and diagnostics
    - Schema language support
5. Consider an online playground for schema editing and generated code preview.

## Key Files

- `src/struct/struct_parse.c`: schema parser and top-level language handling
- `src/gencode/gencode.c`: C code generation logic
- `src/gencode/codes/json_parse.c`: embedded JSON runtime template emitted into generated code
- `src/utils/hash_map.c`: hash map implementation
- `src/utils/hash.c`: shared hashing primitives
- `src/main/main.c`: CLI entry point
- `docs/IMPROVEMENTS.md`: this roadmap and improvement tracker

## Project Direction Notes

1. Phase 1 and Phase 2 remain the highest-value work because parser correctness and type coverage define the tool's usefulness.
2. Phase 4 is a practical prerequisite for a broader 1.0 adoption story.
3. Phase 5 and Phase 6 should be driven by real user demand once the core language and build story are stronger.
4. Current licensing direction remains reasonable: GPL-3.0 for the generator while generated output stays user-controlled.

## Open Design Questions

1. Enum JSON representation should default to strings or integers.
    - ~~Recommended default: strings, with future opt-in control for numeric representation.~~
    - **Resolved:** Strings are the default marshal format. Integer values are accepted during unmarshal for flexibility.
2. Whether the generated parser should support non-standard JSON such as comments or JSON5 features.
    - Recommended default: stay with strict JSON.
3. Whether compatibility should be widened beyond C11.
    - Recommended default: keep C11 unless strong downstream demand justifies a lower baseline.
