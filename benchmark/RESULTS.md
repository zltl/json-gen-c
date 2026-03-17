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
| **yyjson**     | **125 ns** (814 MB/s) | **197 ns** (939 MB/s) | **229 ns** (1.46 GB/s)  |
| rapidjson      | 265 ns (386 MB/s)     | 447 ns (414 MB/s)     | 586 ns (583 MB/s)       |
| cJSON*         | 520 ns (189 MB/s)     | 641 ns (289 MB/s)     | 527 ns (648 MB/s)       |
| **json-gen-c** | **607 ns** (162 MB/s) | **842 ns** (220 MB/s) | **835 ns** (410 MB/s)   |
| json-c         | 1387 ns (74 MB/s)     | 1975 ns (94 MB/s)     | 1553 ns (222 MB/s)      |
| jansson        | 1478 ns (69 MB/s)     | 2184 ns (85 MB/s)     | 2117 ns (162 MB/s)      |

\* cJSON marshal measures only `cJSON_PrintUnformatted()` from a pre-built tree.
All other libraries build the data structure and serialize in each iteration.

### Unmarshal (JSON String to Struct/Data)

| Library        | Scalar (6 fields)     | Nested (12 fields)     | String-Heavy (8 fields) |
|----------------|-----------------------|------------------------|-------------------------|
| **yyjson**     | **104 ns** (977 MB/s) | **154 ns** (1.18 GB/s) | **219 ns** (1.53 GB/s)  |
| rapidjson      | 392 ns (261 MB/s)     | 634 ns (292 MB/s)      | 752 ns (456 MB/s)       |
| cJSON          | 707 ns (145 MB/s)     | 1074 ns (173 MB/s)     | 1030 ns (332 MB/s)      |
| **json-gen-c** | **1074 ns** (92 MB/s) | **1918 ns** (96 MB/s)  | **1800 ns** (190 MB/s)  |
| json-c         | 1674 ns (61 MB/s)     | 2588 ns (72 MB/s)      | 2072 ns (165 MB/s)      |
| jansson        | 1768 ns (58 MB/s)     | 3055 ns (61 MB/s)      | 4351 ns (79 MB/s)       |

### json-gen-c Selective Unmarshal

json-gen-c supports field-mask based selective parsing, skipping unwanted fields:

| Benchmark                    | Full Parse | Selective Parse | Speedup   |
|------------------------------|------------|-----------------|-----------|
| Nested (2 of 12 fields)      | 1918 ns    | 1299 ns         | **1.48x** |
| String-Heavy (2 of 8 fields) | 1800 ns    | 1636 ns         | **1.10x** |

### json-gen-c Array Performance

| Operation | 64-element array   | Per-element |
|-----------|--------------------|-------------|
| Marshal   | 38.9 us (163 MB/s) | 607 ns      |
| Unmarshal | 70.5 us (90 MB/s)  | 1101 ns     |

## Analysis

### Performance Tiers

1. **Tier 1 - Optimized parsers**: yyjson (fastest), rapidjson
   - yyjson is 5-10x faster than json-gen-c through SIMD-optimized parsing
   - rapidjson is 2-3x faster through template-optimized SAX/DOM

2. **Tier 2 - General-purpose**: cJSON, json-gen-c
   - cJSON unmarshal is ~1.5-1.8x faster than json-gen-c (simpler tokenizer)
   - json-gen-c marshal is comparable to cJSON (when cJSON includes tree building)

3. **Tier 3 - Feature-rich**: json-c, jansson
   - json-gen-c is 1.4-2.4x faster than jansson/json-c across all benchmarks

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
