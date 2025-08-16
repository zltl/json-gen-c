# Modern Build System for json-gen-c

This document describes the refactored build system for json-gen-c.

## Overview

The build system has been completely refactored to provide:
- **Centralized configuration** - All build settings in `build.mk`
- **Simplified Makefiles** - Clean, maintainable build rules
- **Better dependency management** - Automatic dependency resolution
- **Parallel building support** - Efficient multi-core compilation
- **Consistent structure** - Unified build patterns across all components

## Build Structure

```
project/
├── build.mk                 # Central build configuration
├── Makefile                 # Main Makefile
├── build/                   # All build artifacts (was target/)
│   ├── bin/                 # Executables
│   ├── lib/                 # Static libraries
│   ├── obj/                 # Object files
│   ├── test/                # Test binaries
│   ├── example/             # Example binary
│   └── benchmark/           # Benchmark binary
├── src/                     # Source code
├── test/                    # Test code with simplified Makefile
├── example/                 # Example code with simplified Makefile
└── benchmark/               # Benchmark code with simplified Makefile
```

## Key Features

### 1. Centralized Configuration (`build.mk`)

All build settings are now centralized in `build.mk`:
- Compiler flags and tools
- Directory structure definitions  
- Build helper functions
- Debug and sanitizer options

### 2. Build Helper Functions

The build system provides several helper functions:
- `compile-c` - Compile C source files
- `compile-cxx` - Compile C++ source files
- `create-lib` - Create static libraries
- `link-exe` - Link C executables
- `link-cxx-exe` - Link C++ executables

### 3. Automatic Dependency Management

- Source files are automatically discovered
- Dependencies are properly ordered (gencode → struct → utils)
- Generated code dependencies are handled correctly

## Build Commands

### Basic Building
```bash
make              # Build main json-gen-c executable
make libs         # Build all libraries
make clean        # Clean all build artifacts
```

### Project Components
```bash
make example      # Build and generate example
make test         # Build and run all tests
make benchmark    # Build benchmark
```

### Debug and Development
```bash
make debug        # Build with debug flags (JSON_DEBUG=1)
make sanitize     # Build with sanitizers (JSON_SANITIZE=1)
make show-config  # Show current build configuration
```

### Parallel Building
```bash
make -j$(nproc)   # Build with all CPU cores
make -j4          # Build with 4 parallel jobs
```

### Testing
```bash
cd test && make run           # Run all tests
cd test && make run-unit      # Run only unit tests
cd test && make run-enhanced  # Run only enhanced tests
cd test && make run-empty-array # Run only empty array tests
```

### Example and Benchmark
```bash
cd example && make run        # Run example program
cd benchmark && make run      # Run benchmark
```

## Installation
```bash
make install               # Install to /usr/ (default)
make install DEST=/usr/local  # Install to custom location
make uninstall             # Remove installation
```

## Build Configuration Variables

### Paths
- `ROOT_DIR` - Project root directory (auto-detected)
- `BUILD_DIR` - Build output directory (default: `build/`)
- `TARGET_DIR` - Alias for `BUILD_DIR` (compatibility)

### Compiler Tools
- `CC` - C compiler (default: `gcc`)
- `CXX` - C++ compiler (default: `g++`) 
- `AR` - Archiver (default: `ar`)

### Debug Options
- `JSON_DEBUG=1` - Enable debug output and flags
- `JSON_SANITIZE=1` - Enable address sanitizer

### Install Location
- `DEST` - Installation prefix (default: `/usr/`)

## Migration from Old Build System

### Changes Made
1. **Removed sub-directory Makefiles** - All source compilation is now handled by the main Makefile
2. **Unified build directory** - Changed from `target/` to `build/` for clarity
3. **Simplified test building** - Test Makefile now uses helper functions
4. **Better parallel support** - Dependencies are properly declared for parallel builds
5. **Cleaner output** - More informative build messages

### Backward Compatibility
- Most make targets remain the same (`make`, `make test`, `make example`, etc.)
- Environment variables (`JSON_DEBUG`, `JSON_SANITIZE`) work as before
- Installation process unchanged

## Troubleshooting

### Common Issues

1. **Build fails with missing directories**
   - Solution: Run `make clean` first

2. **Parallel build fails**
   - Solution: Dependencies should be properly declared, but if issues persist, build without `-j`

3. **Tests fail to find generated files**  
   - Solution: Ensure `json-gen-c` executable builds successfully first

4. **Permission denied during install**
   - Solution: Use `sudo make install` or set `DEST` to a writable location

### Getting Help

1. Show current configuration: `make show-config`
2. See what make would do: `make -n`
3. Build with verbose output: `make V=1` (if implemented)
4. Check individual components: `make example` or `make test`

## Performance

The new build system provides several performance improvements:
- **Parallel compilation** - Full support for `-j` flag
- **Incremental builds** - Only rebuild what's necessary
- **Faster dependency resolution** - Automatic source discovery
- **Reduced redundancy** - No more recursive make calls to subdirectories

## Future Enhancements

Potential future improvements:
- CMake integration for cross-platform support
- Automated testing integration (CI/CD)
- Package generation (RPM, DEB)
- Cross-compilation support
- Build caching for faster rebuilds
