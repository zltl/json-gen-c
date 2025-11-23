# Project Improvement Plan

This document outlines identified areas for repair, optimization, and improvement in the `json-gen-c` project.

## 1. Critical Fixes

### 1.1. Implement Hash Map Resizing
**Location:** `src/utils/hash_map.c`

**Issue:**
The function `hash_map_resize_if_needed` currently contains a TODO and does not perform any resizing.
```c
static int hash_map_resize_if_needed(struct hash_map* map) {
    // ...
    // TODO: Implement hash table expansion
    // ...
    return JSON_GEN_SUCCESS;
}
```
**Impact:**
As elements are added, the load factor increases, causing hash collisions and degrading lookup performance from O(1) to O(N). For large input schemas, this will significantly slow down compilation.

**Solution:**
Implement the resizing logic:
1.  Allocate a new bucket array (e.g., double the size).
2.  Rehash all existing entries into the new buckets.
3.  Free the old bucket array.
4.  Update `map->buckets` and `map->bucket_count`.

## 2. Performance Optimizations

### 2.1. Optimize `hash_2s` Function
**Location:** `src/gencode/gencode.c`

**Issue:**
The `hash_2s` function allocates a new temporary string just to combine two keys for hashing.
```c
inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    // ...
    sstr_t tmp = sstr_dup(key1);
    sstr_append_of(tmp, "#", 1);
    sstr_append(tmp, key2);
    // ...
    sstr_free(tmp);
    return res;
}
```
**Impact:**
Frequent memory allocation and deallocation (`malloc`/`free`) during code generation adds unnecessary overhead.

**Solution:**
Implement a chaining hash function that doesn't require string concatenation.
```c
inline static unsigned int hash_2s(sstr_t key1, sstr_t key2) {
    unsigned int h = 0xbc9f1d34;
    h = hash_s(sstr_cstr(key1), sstr_length(key1), h);
    // Mix in a separator to avoid "ab" + "c" == "a" + "bc" collisions if needed, 
    // though for struct fields this might not be strictly necessary if delimiters are implied.
    // Better: hash a separator byte.
    h = hash_s("#", 1, h); 
    h = hash_s(sstr_cstr(key2), sstr_length(key2), h);
    return h;
}
```
*Note: `hash_s` would need to be modified to accept a seed.*

### 2.2. Unify Hash Functions
**Location:** `src/gencode/gencode.c` and `src/utils/hash_map.c`

**Issue:**
The MurmurHash-like algorithm is duplicated. `src/gencode/gencode.c` has `hash_s` and `src/utils/hash_map.c` has `hash`.

**Solution:**
Move the hash implementation to a common utility header (e.g., `src/utils/hash.h` or expose it in `src/utils/hash_map.h`) and reuse it across the project.

## 3. Code Quality Improvements

### 3.1. Use `stdbool.h` (Completed)
**Location:** `src/utils/sstr.c` (and others)

**Issue:**
`char_to_hex` uses `int cap` as a boolean flag.
```c
static void char_to_hex(unsigned char c, unsigned char* buf, int cap)
```

**Solution:**
Include `<stdbool.h>` and use `bool` type for clarity.
**Status:** Completed. Refactored `sstr.c` and `sstr.h` to use `bool` for boolean parameters and flags.

### 3.2. Robust Argument Parsing (Completed)
**Location:** `src/main/main.c`

**Issue:**
Manual `argv` parsing is brittle and hard to extend.

**Solution:**
Consider using `getopt_long` (standard on Linux) or a lightweight argument parsing library for better handling of flags, help messages, and validation.
**Status:** Completed. Implemented `getopt_long_only` to support existing `-in` and `-out` flags as well as standard short/long options.

### 3.3. Error Handling Consistency (Completed)
**Location:** `src/main/main.c`

**Issue:**
`usage()` prints to `stdout`.

**Solution:**
Error messages and usage information (when triggered by an error) should ideally go to `stderr`.
**Status:** Completed. Modified `usage()` to accept a `FILE* stream` and updated calls to send error messages to `stderr` and help messages to `stdout`.

### 3.4. Fix Off-by-One Error in Parser
**Location:** `src/struct/struct_parse.c`

**Issue:**
The `next_token_` function prematurely returns `TOKEN_EOF` when `parser->pos.offset` reaches `length - 1`.
```c
    if (parser->pos.offset >= (long)sstr_length(content) - 1) {
        parser->pos.offset++;
        token->type = TOKEN_EOF;
        return TOKEN_EOF;
    }
```
This causes the last character of the file to be ignored. If the file ends with a significant character (like `}` or `;`) instead of a newline, parsing may fail.

**Solution:**
Change the condition to:
```c
    if (parser->pos.offset >= (long)sstr_length(content)) {
```

### 3.5. Robust File I/O (Completed)
**Location:** `src/utils/io.c`

**Issue:**
`read_file` and `write_file` do not check the return values of `fread` and `fwrite`. `ftell` return value is also not checked.

**Solution:**
Add error checking for file operations and return appropriate error codes.
**Status:** Completed. Added error checking for `fopen`, `fseek`, `ftell`, `fread`, and `fwrite`.

## 4. Feature Requests

### 4.1. Support for More Types
Currently, the project supports basic types and structs. Adding support for:
*   Enums
*   Fixed-size arrays (e.g., `int data[10];`)
*   Boolean type (mapping to `bool` or `int`)

### 4.2. Custom Allocators
Allow the generated code to use custom memory allocators instead of `malloc`/`free`, which is useful for embedded systems or high-performance applications.
