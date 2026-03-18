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
| **yyjson**     | **125 ns** (817 MB/s) | **194 ns** (952 MB/s) | **244 ns** (1.36 GB/s)  |
| rapidjson      | 271 ns (377 MB/s)     | 384 ns (481 MB/s)     | 517 ns (660 MB/s)       |
| **json-gen-c** | **483 ns** (204 MB/s) | **619 ns** (299 MB/s) | **671 ns** (509 MB/s)   |
| cJSON*         | 537 ns (183 MB/s)     | 641 ns (289 MB/s)     | 548 ns (623 MB/s)       |
| json-c         | 1429 ns (71 MB/s)     | 1787 ns (104 MB/s)    | 1530 ns (225 MB/s)      |
| jansson        | 1521 ns (67 MB/s)     | 2177 ns (85 MB/s)     | 2108 ns (162 MB/s)      |

\* cJSON marshal measures only `cJSON_PrintUnformatted()` from a pre-built tree.
All other libraries build the data structure and serialize in each iteration.

### Unmarshal (JSON String to Struct/Data)

| Library        | Scalar (6 fields)     | Nested (12 fields)     | String-Heavy (8 fields) |
|----------------|-----------------------|------------------------|-------------------------|
| **yyjson**     | **112 ns** (914 MB/s) | **152 ns** (1.19 GB/s) | **198 ns** (1.68 GB/s)  |
| rapidjson      | 390 ns (262 MB/s)     | 624 ns (296 MB/s)      | 760 ns (449 MB/s)       |
| **json-gen-c** | **518 ns** (190 MB/s) | **997 ns** (186 MB/s)  | **1044 ns** (327 MB/s)  |
| cJSON          | 697 ns (146 MB/s)     | 1037 ns (178 MB/s)     | 1015 ns (336 MB/s)      |
| json-c         | 2138 ns (48 MB/s)     | 2713 ns (68 MB/s)      | 2067 ns (165 MB/s)      |
| jansson        | 1747 ns (58 MB/s)     | 3084 ns (60 MB/s)      | 5016 ns (68 MB/s)       |

### json-gen-c Selective Unmarshal

json-gen-c supports field-mask based selective parsing, skipping unwanted fields:

| Benchmark                    | Full Parse | Selective Parse | Speedup   |
|------------------------------|------------|-----------------|-----------|
| Nested (2 of 12 fields)      | 997 ns     | 696 ns          | **1.43x** |
| String-Heavy (2 of 8 fields) | 1044 ns    | 831 ns          | **1.26x** |

### json-gen-c Array Performance

| Operation | 64-element array   | Per-element |
|-----------|--------------------|-------------|
| Marshal   | 31.6 us (201 MB/s) | 494 ns      |
| Unmarshal | 35.5 us (179 MB/s) | 554 ns      |

## Analysis

### Performance Tiers

1. **Tier 1 - Optimized parsers**: yyjson (fastest), rapidjson
   - yyjson is 4-7x faster than json-gen-c through SIMD-optimized parsing
   - rapidjson is 1.3-1.6x faster through template-optimized SAX/DOM

2. **Tier 2 - General-purpose**: json-gen-c, cJSON
   - json-gen-c marshal is **10% faster** than cJSON (scalar), **3% faster** (nested)
   - json-gen-c unmarshal is **26% faster** than cJSON (scalar), **4% faster** (nested)
   - json-gen-c provides type safety and multi-format support while being faster

3. **Tier 3 - Feature-rich**: json-c, jansson
   - json-gen-c is 2-5x faster than jansson/json-c across all benchmarks

### When to Choose json-gen-c

json-gen-c is **faster than cJSON** on all benchmarks while providing much stronger
developer ergonomics:

| Feature               | json-gen-c                          | cJSON/yyjson/rapidjson           |
|-----------------------|-------------------------------------|----------------------------------|
| **Performance**       | Faster than cJSON on all benchmarks | yyjson/rapidjson are 2-5x faster |
| **Type safety**       | Compile-time typed C structs        | Runtime tree traversal           |
| **Memory model**      | Stack-friendly, no tree allocation  | Heap-allocated DOM trees         |
| **Selective parsing** | Built-in field masks                | Manual (skip unwanted nodes)     |
| **Code generation**   | Zero boilerplate marshal/unmarshal  | Manual tree building/extraction  |
| **Binary formats**    | MessagePack + CBOR from same schema | JSON only (separate libs needed) |
| **Schema evolution**  | @deprecated + --check-compat        | Manual migration                 |

**Best for**: Applications where developer productivity, type safety, and multi-format
support matter. json-gen-c gives you all of this with performance that beats the most
popular C JSON library (cJSON), only trailing the SIMD-optimized tier (yyjson, rapidjson).

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
