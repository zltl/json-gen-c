# Copy and Move API Proposal

## Status

Proposed.

## Summary

This proposal introduces generated in-memory copy and move helpers for every
generated struct and oneof type in json-gen-c.

The initial scope is intentionally narrow:

- Generate full deep-copy helpers.
- Generate ownership-transfer move helpers.
- Keep the memory model aligned with the existing `_init()` and `_clear()`
  APIs.
- Defer partial-update and merge semantics to a later proposal.

This proposal does not recommend reusing the existing selective-unmarshal
`field_mask` and `json_nested_mask` APIs for object-to-object copy.

## Motivation

Today, users who need to apply one generated object to another have two main
options:

1. Copy fields manually and carefully free overwritten storage.
2. Marshal the source object back to JSON and then unmarshal it into the
   destination object.

Both approaches are unsatisfactory.

Manual copying is verbose, easy to get wrong, and must be updated every time a
schema changes. The JSON round-trip path is simpler at the call site, but it is
wasteful and does not match the actual problem, which is in-memory data
transfer.

This gap is already visible in the generated C++ wrapper. Move construction and
move assignment are implemented as raw ownership transfer, while copy
construction and copy assignment still rely on marshal/unmarshal round-trips.
That mismatch suggests the C generator is missing a native copy primitive.

## Goals

- Generate a stable per-type C API for deep copy.
- Generate a stable per-type C API for ownership transfer.
- Cover all generated field categories already supported by marshal and
  unmarshal:
  - scalar numeric types and `bool`
  - `sstr_t`
  - enums
  - nested structs
  - oneof values
  - fixed-size arrays
  - dynamic arrays
  - maps
  - `optional` and `nullable` presence state
- Keep failure behavior explicit and easy to reason about.
- Provide a clean foundation for later C++ wrapper improvements.

## Non-Goals

- Partial field copy in V1.
- Patch or merge semantics in V1.
- A stable field-numbering system.
- Reusing JSON selective-unmarshal masks as the primary in-memory copy API.
- Preserving the previous contents of `dest` on copy failure.

## Proposed API

For each generated struct:

```c
int MyStruct_copy(struct MyStruct* dest, const struct MyStruct* src);
int MyStruct_move(struct MyStruct* dest, struct MyStruct* src);
```

For each generated oneof:

```c
int MyOneof_copy(struct MyOneof* dest, const struct MyOneof* src);
int MyOneof_move(struct MyOneof* dest, struct MyOneof* src);
```

Return values follow the existing generator convention:

- `0` on success
- `-1` on failure

## Behavioral Contract

### Preconditions

- `dest` and `src` must be non-NULL.
- `dest` must point to a valid generated object that can be safely passed to the
  corresponding `_clear()` function.
- `src` must point to a valid generated object.

### Common Rules

- If `dest == src`, the function returns `0` immediately.
- The generated helper owns all cleanup of overwritten destination state.
- The caller does not need to manually clear selected subfields before calling
  `*_copy()` or `*_move()`.

### Copy Semantics

`*_copy()` performs a deep copy of the full logical object graph represented by
`src`.

This includes:

- duplicating `sstr_t` storage
- allocating and copying dynamic arrays
- allocating and copying map entry arrays
- recursively copying nested structs
- copying oneof tag state and the active variant payload
- copying `has_<field>` flags for `optional` and `nullable` fields

The destination object is fully independent from the source object after a
successful copy.

### Move Semantics

`*_move()` transfers ownership of all movable storage from `src` into `dest`.

The primary design target is to avoid nested allocations during the move path.
In practice, the implementation should clear `dest`, transfer ownership from
`src`, and then leave `src` in a valid cleared state.

This proposal deliberately uses "cleared state" rather than "init-equivalent
state" for moved-from objects. Re-applying schema defaults after move would
reintroduce allocations for cases such as default `sstr_t` values, which would
undercut the main performance benefit of move support.

After a successful move:

- `dest` owns the transferred resources
- `src` is safe to pass to `*_clear()`
- `src` may be reused after the caller reinitializes or repopulates it

### Failure Semantics

`*_copy()` may fail because of allocation failure.

If that happens:

- the function returns `-1`
- `dest` is left in a valid cleared state
- the previous value of `dest` is not preserved

This rule keeps the generated implementation simple, predictable, and aligned
with the current library style, which already favors explicit cleanup over
transactional rollback.

`*_move()` should not require nested allocations and therefore should not fail
in normal operation once argument validation passes.

## Why Not Use Field Masks for Copy

The current selective-unmarshal APIs are useful for parsing chosen JSON keys
into an existing object, but they are not a good default model for in-memory
copy.

There are three main reasons.

### 1. The Current API Is Parse-Oriented

Selective unmarshal is built around JSON input. Selected fields are cleared and
then repopulated from parsed tokens. Presence tracking is also driven by the
parser.

That is the correct model for `json_unmarshal_selected_*()`, but it is not the
same problem as copying one already-materialized object into another.

### 2. Nested Selection Is Too Heavy for a Default Object API

Deep selection currently requires callers to construct `json_nested_mask`
tables. That is acceptable for advanced parse-time filtering, but it is too much
machinery for the common case of "copy this object" or "move this object".

### 3. Merge Is a Separate Semantics Problem

The original feature request also discussed partial updates. That is a real use
case, but it is not equivalent to copy.

A merge API needs a clear answer to a different question: how does the library
distinguish between "field not present in the patch" and "field present with a
zero or empty value"? That question is partly addressed by `optional` and
`nullable`, but not for all field shapes.

Because of that, merge should be treated as a separate proposal rather than
folded into V1.

## Alternatives Considered

### Reuse marshal/unmarshal round-trip in generated C APIs

Rejected.

That would preserve the current inefficiency and would not solve the underlying
problem. It would also produce a misleading API: users would expect in-memory
copy, but the generated code would still pay serialization costs.

### Add `*_copy_selected()` and `*_move_selected()` first

Rejected for V1.

The core missing capability is full-object copy and full-object move. Adding a
mask-based API first would push complexity onto users before the foundational
resource-management primitives exist.

### Leave move source in init-equivalent state

Rejected.

That would require re-applying defaults after move and could force allocations
on the moved-from object. A valid cleared state better matches the intended
performance profile.

## Implementation Plan

### Phase 1: Finalize Contract

Before code changes begin, lock down these points:

- moved-from objects are cleared, not reinitialized with defaults
- copy failure leaves `dest` cleared
- self-copy and self-move are explicit no-op success cases
- V1 includes struct and oneof helpers only

This phase should end with agreement on the exact behavioral contract.

### Phase 2: Add Generator Entry Points

Extend the generator so that struct and oneof declarations emit the new
function prototypes into generated headers and the matching definitions into the
generated source.

Recommended insertion points:

- alongside other per-struct declarations in `src/gencode/gencode.c`
- inside the existing per-struct generation flow after `_init()` and `_clear()`
- alongside oneof init and clear generation for tagged unions

### Phase 3: Introduce Field-Level Emit Helpers

Add internal generator helpers that emit copy and move logic by field category.
At minimum, separate helpers are recommended for:

- scalars and enums
- `sstr_t`
- fixed-size arrays
- dynamic arrays
- maps
- nested structs
- oneof fields

This keeps the generated-code templates maintainable and avoids turning a single
generation function into a large monolithic branch tree.

### Phase 4: Implement Complex Resource Paths

The following cases need special handling:

- dynamic arrays of `sstr_t`, structs, and oneofs
- maps with `sstr_t` keys and non-trivial value types
- nested structs inside arrays and maps
- oneof values, including arrays of oneof values
- `optional` and `nullable` fields whose `has_<field>` state must travel with
  the copied logical value

This is the highest-risk implementation phase.

### Phase 5: Add Focused Tests

Create a dedicated test target for copy and move behavior.

The existing `test/test.json-gen-c` schema already contains the core building
blocks needed for initial coverage, including maps, optional fields, nullable
fields, and oneof types. Reusing those definitions will keep the first test pass
small and focused.

Recommended test file:

```text
test/copy_move_test.cc
```

Recommended build integration:

- add a standalone `copy_move_test` target to `test/Makefile`
- run that target first for narrow validation
- then run the full test suite

### Phase 6: Update the C++ Wrapper

Once the C helpers are stable, update the generated C++ wrapper so that:

- copy construction uses generated `*_copy()` helpers
- copy assignment uses generated `*_copy()` helpers
- move construction uses generated `*_move()` helpers
- move assignment uses generated `*_move()` helpers

This removes the current marshal/unmarshal round-trip from C++ copy paths and
ensures C and C++ resource semantics stay aligned.

### Phase 7: Document the New API

After the implementation lands, document:

- the new generated functions
- overwrite semantics for `dest`
- cleared-state semantics for moved-from objects
- failure behavior for copy
- why merge is not part of the initial release

## Test Plan

The first test pass should focus on correctness and memory ownership.

Minimum recommended cases:

- deep copy of `sstr_t` fields proves independence between source and
  destination
- move of dynamic arrays proves ownership transfer without double free
- map copy duplicates both keys and values correctly
- nested struct copy is recursive and leaves no shared ownership
- oneof copy and move preserve both tag and active payload
- `optional` and `nullable` flags remain consistent with copied data
- self-copy and self-move succeed without corruption
- copy failure paths leave `dest` clearable

Recommended validation order:

1. Build the generator.
2. Build and run the dedicated copy/move test target.
3. Run `make test` for full regression coverage.
4. Run a sanitizer-backed test pass for additional memory validation when
   needed.

## Deferred Work

The following items are explicitly deferred until after V1:

- `*_merge()` APIs
- `*_copy_selected()` and `*_move_selected()` APIs
- top-level or nested field-mask driven in-memory copy
- feature flags to disable copy/move generation for code-size reasons

Those features may still be worth adding later, but they should be evaluated on
top of a stable full-object copy and move foundation.

