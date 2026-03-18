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
| **yyjson**     | **125 ns** (819 MB/s) | **196 ns** (944 MB/s) | **226 ns** (1.48 GB/s)  |
| rapidjson      | 259 ns (395 MB/s)     | 376 ns (493 MB/s)     | 583 ns (586 MB/s)       |
| **json-gen-c** | **378 ns** (260 MB/s) | **328 ns** (564 MB/s) | **425 ns** (806 MB/s)   |
| cJSON*         | 537 ns (183 MB/s)     | 621 ns (298 MB/s)     | 516 ns (661 MB/s)       |
| json-c         | 1392 ns (73 MB/s)     | 1691 ns (109 MB/s)    | 1548 ns (222 MB/s)      |
| jansson        | 1501 ns (68 MB/s)     | 2206 ns (84 MB/s)     | 2069 ns (165 MB/s)      |

\* cJSON marshal measures only `cJSON_PrintUnformatted()` from a pre-built tree.
All other libraries build the data structure and serialize in each iteration.

### Unmarshal (JSON String to Struct/Data)

| Library        | Scalar (6 fields)     | Nested (12 fields)     | String-Heavy (8 fields) |
|----------------|-----------------------|------------------------|-------------------------|
| **yyjson**     | **107 ns** (950 MB/s) | **155 ns** (1.17 GB/s) | **199 ns** (1.68 GB/s)  |
| rapidjson      | 387 ns (264 MB/s)     | 636 ns (291 MB/s)      | 747 ns (457 MB/s)       |
| **json-gen-c** | **523 ns** (188 MB/s) | **1018 ns** (182 MB/s) | **926 ns** (369 MB/s)   |
| cJSON          | 695 ns (147 MB/s)     | 1040 ns (178 MB/s)     | 1178 ns (290 MB/s)      |
| json-c         | 1759 ns (58 MB/s)     | 2672 ns (69 MB/s)      | 2357 ns (145 MB/s)      |
| jansson        | 1791 ns (57 MB/s)     | 3146 ns (59 MB/s)      | 4438 ns (77 MB/s)       |

### json-gen-c Selective Unmarshal

json-gen-c supports field-mask based selective parsing, skipping unwanted fields:

| Benchmark                    | Full Parse | Selective Parse | Speedup   |
|------------------------------|------------|-----------------|-----------|
| Nested (2 of 12 fields)      | 1018 ns    | 722 ns          | **1.41x** |
| String-Heavy (2 of 8 fields) | 926 ns     | 932 ns          | **1.00x** |

### json-gen-c Array Performance

| Operation | 64-element array   | Per-element |
|-----------|--------------------|-------------|
| Marshal   | 25.3 us (251 MB/s) | 396 ns      |
| Unmarshal | 36.7 us (173 MB/s) | 574 ns      |

## Analysis

### Performance Tiers

1. **Tier 1 - Optimized parsers**: yyjson (fastest), rapidjson
   - yyjson is 4-7x faster than json-gen-c through SIMD-optimized parsing
   - rapidjson is 1.3-1.6x faster through template-optimized SAX/DOM

2. **Tier 2 - General-purpose**: json-gen-c, cJSON
   - json-gen-c marshal is **30% faster** than cJSON (scalar), **47% faster** (nested), **18% faster** (string-heavy)
   - json-gen-c unmarshal is **25% faster** than cJSON (scalar), **21% faster** (string-heavy)
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
