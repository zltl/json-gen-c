# Build System

json-gen-c supports three build systems: **GNU Make** (primary), **CMake**, and **Meson**.
All three produce the same `json-gen-c` executable and install the same artifacts.

## GNU Make (default)

```bash
make -j$(nproc)
sudo make install        # installs to /usr/local by default
```

Useful targets:

| Target | Description |
|--------|-------------|
| `make` | Build the `json-gen-c` binary |
| `make test` | Build and run the test suite |
| `make example` | Build the example program |
| `make benchmark` | Build the benchmark |
| `make debug` | Debug build (`-O0 -g`) |
| `make sanitize` | Build with AddressSanitizer + UBSan |
| `make install` | Install binary and man page |
| `make show-config` | Print build configuration |

## CMake

Requires CMake 3.16+ and `xxd` (usually shipped with `vim`).

### Building

```bash
mkdir build-cmake && cd build-cmake
cmake ..
cmake --build . -j$(nproc)
```

### Testing

```bash
ctest --output-on-failure
```

On multi-config generators such as Visual Studio, use:

```bash
ctest -C Release --output-on-failure
```

### Installing

```bash
sudo cmake --install .
# Or install to a custom prefix:
cmake --install . --prefix /opt/json-gen-c
```

Installed files:

| File | Location |
|------|----------|
| `json-gen-c` | `<prefix>/bin/` |
| `json-gen-c.1` | `<prefix>/share/man/man1/` |
| CMake package config | `<prefix>/lib/cmake/JsonGenC/` |
| pkg-config file | `<prefix>/lib/pkgconfig/json-gen-c.pc` |

### Using from a CMake project

After installing, downstream projects can use `find_package`:

```cmake
find_package(JsonGenC REQUIRED)

# Generate C code from a schema
json_gen_c_generate(
    TARGET my_app
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/structs.json-gen-c
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
)

add_executable(my_app main.c)
```

The `json_gen_c_generate()` macro:
- Runs `json-gen-c -in <SCHEMA> -out <OUTPUT_DIR>` at build time.
- Adds the generated `json.gen.c`, `json.gen.h`, `sstr.c`, and `sstr.h` to the specified `TARGET`.
- Rebuilds automatically when the schema file changes.

### pkg-config

After installation, the tool can be discovered via pkg-config:

```bash
pkg-config --modversion json-gen-c   # prints version
pkg-config --variable=generator json-gen-c  # prints path to binary
```

This is useful in Makefile-based or Autotools projects:

```makefile
JSON_GEN_C := $(shell pkg-config --variable=generator json-gen-c)
json.gen.c json.gen.h: structs.json-gen-c
	$(JSON_GEN_C) -in $< -out $(dir $@)
```

## Meson

Requires Meson 0.56+ and `xxd`.

### Building

```bash
meson setup builddir
ninja -C builddir
```

### Installing

```bash
sudo ninja -C builddir install
```

Installs the `json-gen-c` binary and man page.

## Requirements

All build systems require:

- A C11 compiler (GCC or Clang recommended)
- POSIX threads (`-lpthread`)
- `xxd` command (for embedding runtime code templates)

For running tests (Make only):

- A C++17 compiler (for Google Test)
