# Benchmark Results

Comprehensive comparison of json-gen-c against five popular JSON libraries.

## Environment

- **CPU**: 2x 2250 MHz (L1 32 KiB, L2 1 MiB, L3 16 MiB)
- **Compiler**: GCC with -O2 -DNDEBUG
- **OS**: Linux (x86_64)
- **Framework**: Google Benchmark

## Libraries Tested

| Library                                             | Version | Language | Type                           |
|-----------------------------------------------------|---------|----------|--------------------------------|
| [json-gen-c](https://github.com/petermz/json-gen-c) | 0.9.0   | C        | Code generator (typed structs) |
| [cJSON](https://github.com/DaveGamble/cJSON)        | 1.7.17  | C        | Tree-based DOM                 |
| [yyjson](https://github.com/ibireme/yyjson)         | latest  | C        | Optimized DOM                  |
| [rapidjson](https://github.com/Tencent/rapidjson)   | 1.1     | C++      | SAX/DOM hybrid                 |
| [jansson](https://github.com/akheron/jansson)       | 2.14    | C        | Tree-based DOM                 |
| [json-c](https://github.com/json-c/json-c)          | 0.17    | C        | Tree-based DOM                 |

## Results Summary

### Marshal (Struct/Data to JSON String)

| Library        | Scalar (6 fields)     | Nested (12 fields)    | String-Heavy (8 fields) |
|----------------|-----------------------|-----------------------|-------------------------|
| **yyjson**     | **128 ns** (797 MB/s) | **203 ns** (913 MB/s) | **233 ns** (1.43 GB/s)  |
| rapidjson      | 263 ns (388 MB/s)     | 384 ns (482 MB/s)     | 593 ns (576 MB/s)       |
| **json-gen-c** | **462 ns** (212 MB/s) | **600 ns** (308 MB/s) | **633 ns** (539 MB/s)   |
| cJSON*         | 534 ns (184 MB/s)     | 637 ns (290 MB/s)     | 541 ns (631 MB/s)       |
| json-c         | 1407 ns (73 MB/s)     | 1738 ns (106 MB/s)    | 1565 ns (220 MB/s)      |
| jansson        | 1569 ns (65 MB/s)     | 2306 ns (80 MB/s)     | 2150 ns (159 MB/s)      |

\* cJSON marshal measures only `cJSON_PrintUnformatted()` from a pre-built tree.
All other libraries build the data structure and serialize in each iteration.

### Unmarshal (JSON String to Struct/Data)

| Library        | Scalar (6 fields)     | Nested (12 fields)     | String-Heavy (8 fields) |
|----------------|-----------------------|------------------------|-------------------------|
| **yyjson**     | **114 ns** (898 MB/s) | **161 ns** (1.12 GB/s) | **218 ns** (1.53 GB/s)  |
| rapidjson      | 416 ns (245 MB/s)     | 655 ns (282 MB/s)      | 781 ns (437 MB/s)       |
| cJSON          | 700 ns (146 MB/s)     | 1049 ns (176 MB/s)     | 1048 ns (326 MB/s)      |
| **json-gen-c** | **747 ns** (131 MB/s) | **1420 ns** (130 MB/s) | **1357 ns** (252 MB/s)  |
| json-c         | 2108 ns (48 MB/s)     | 2596 ns (71 MB/s)      | 2095 ns (163 MB/s)      |
| jansson        | 1873 ns (54 MB/s)     | 3315 ns (56 MB/s)      | 4567 ns (75 MB/s)       |

### json-gen-c Selective Unmarshal

json-gen-c supports field-mask based selective parsing, skipping unwanted fields:

| Benchmark                    | Full Parse | Selective Parse | Speedup   |
|------------------------------|------------|-----------------|-----------|
| Nested (2 of 12 fields)      | 1420 ns    | 1043 ns         | **1.36x** |
| String-Heavy (2 of 8 fields) | 1357 ns    | 1198 ns         | **1.13x** |

### json-gen-c Array Performance

| Operation | 64-element array   | Per-element |
|-----------|--------------------|-------------|
| Marshal   | 31.8 us (200 MB/s) | 497 ns      |
| Unmarshal | 49.0 us (130 MB/s) | 766 ns      |

## Analysis

### Performance Tiers

1. **Tier 1 - Optimized parsers**: yyjson (fastest), rapidjson
   - yyjson is 4-7x faster than json-gen-c through SIMD-optimized parsing
   - rapidjson is 1.2-1.8x faster through template-optimized SAX/DOM

2. **Tier 2 - General-purpose**: json-gen-c, cJSON
   - json-gen-c marshal is **13% faster** than cJSON (scalar), **6% faster** (nested)
   - cJSON unmarshal is 7-35% faster than json-gen-c (simpler tokenizer)
   - json-gen-c provides type safety and multi-format support at near-parity speed

3. **Tier 3 - Feature-rich**: json-c, jansson
   - json-gen-c is 1.5-3.4x faster than jansson/json-c across all benchmarks

### When to Choose json-gen-c

json-gen-c is not the fastest JSON library. Its value proposition is different:

| Feature               | json-gen-c                          | cJSON/yyjson/rapidjson           |
|-----------------------|-------------------------------------|----------------------------------|
| **Type safety**       | Compile-time typed C structs        | Runtime tree traversal           |
| **Memory model**      | Stack-friendly, no tree allocation  | Heap-allocated DOM trees         |
| **Selective parsing** | Built-in field masks                | Manual (skip unwanted nodes)     |
| **Code generation**   | Zero boilerplate marshal/unmarshal  | Manual tree building/extraction  |
| **Binary formats**    | MessagePack + CBOR from same schema | JSON only (separate libs needed) |
| **Schema evolution**  | @deprecated + --check-compat        | Manual migration                 |

**Best for**: Applications where developer productivity, type safety, and multi-format
support matter more than raw parsing throughput. Particularly strong when you need
the same data structures serialized to JSON, MessagePack, and CBOR.

## Reproducing

### One-command reproduction (Debian/Ubuntu)

```bash
make benchmark-repro
```

This target:

- installs missing distro packages with `apt-get`
- clones `yyjson` into `benchmark/yyjson/`
- installs yyjson locally into `benchmark/.deps/prefix/`
- builds the project and benchmark suite with `-O2 -DNDEBUG`
- runs the benchmark executable

`benchmark/yyjson/` and `benchmark/.deps/` are local reproduction caches and are
intentionally gitignored.

### Manual reproduction

```bash
# Install dependencies
sudo apt install libcjson-dev libjansson-dev libjson-c-dev rapidjson-dev libbenchmark-dev

# Build yyjson from source
git clone https://github.com/ibireme/yyjson && cd yyjson
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
sudo cmake --install build

# Build and run benchmarks
cd json-gen-c
cmake -S . -B build && cmake --build build
CFLAGS="-O2 -DNDEBUG" CXXFLAGS="-O2 -DNDEBUG" make -C benchmark all
LD_LIBRARY_PATH=/usr/local/lib make -C benchmark run
```
