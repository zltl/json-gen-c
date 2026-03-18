# Benchmark Makefile for json-gen-c
include ../build.mk

.PHONY: all clean deps run

BENCH_PREFIX ?= $(abspath .deps/prefix)

BENCH_INCLUDE_FLAGS :=
BENCH_LINK_FLAGS :=

ifneq ($(wildcard $(BENCH_PREFIX)/include),)
BENCH_INCLUDE_FLAGS += -I$(BENCH_PREFIX)/include
endif

ifneq ($(wildcard $(BENCH_PREFIX)/lib),)
BENCH_LINK_FLAGS += -L$(BENCH_PREFIX)/lib -Wl,-rpath,$(BENCH_PREFIX)/lib
endif

# Benchmark build directory
BENCH_BUILD := $(BUILD_DIR)/benchmark

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(BENCH_BUILD)/%.o,$(GENERATED_SOURCES))

# Benchmark sources and objects
BENCH_SOURCE := json_bench.cc
BENCH_OBJECT := $(BENCH_BUILD)/json_bench.o

# Separate TU sources (jansson/json-c conflict on json_object)
BENCH_JANSSON_SRC := bench_jansson.cc
BENCH_JANSSON_OBJ := $(BENCH_BUILD)/bench_jansson.o
BENCH_JSONC_SRC   := bench_jsonc.cc
BENCH_JSONC_OBJ   := $(BENCH_BUILD)/bench_jsonc.o

# Benchmark executable
BENCHMARK := $(BENCH_BUILD)/json_bench

#==============================================================================
# Build rules
#==============================================================================

all: $(BENCHMARK)

deps:
	@./bootstrap_deps.sh

# Generate benchmark structures
json.gen.c sstr.c: structs.json-gen-c $(JSON_GEN_C)
	@echo "Generating benchmark structures..."
	$(JSON_GEN_C) -in structs.json-gen-c -out .

# Compile generated files (include project src/ for utils/error_codes.h)
$(foreach src,$(GENERATED_SOURCES),$(eval $(call compile-c,$(src),$(patsubst %.c,$(BENCH_BUILD)/%.o,$(src)),-I. -I$(ROOT_DIR)/src)))

# Compile benchmark main
$(eval $(call compile-cxx,$(BENCH_SOURCE),$(BENCH_OBJECT),-I. $(BENCH_INCLUDE_FLAGS)))

# Compile jansson and json-c benchmarks (separate TUs)
$(eval $(call compile-cxx,$(BENCH_JANSSON_SRC),$(BENCH_JANSSON_OBJ),$(BENCH_INCLUDE_FLAGS)))
$(eval $(call compile-cxx,$(BENCH_JSONC_SRC),$(BENCH_JSONC_OBJ),$(BENCH_INCLUDE_FLAGS)))

# Make benchmark object depend on generated files
$(BENCH_OBJECT): json.gen.c

# Link benchmark executable
$(BENCHMARK): $(BENCH_OBJECT) $(BENCH_JANSSON_OBJ) $(BENCH_JSONC_OBJ) $(GENERATED_OBJECTS)
	@echo "Linking benchmark: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(BENCH_LINK_FLAGS) $^ -o $@ -lbenchmark -lcjson -lyyjson -ljansson -ljson-c -lpthread

#==============================================================================
# Run benchmark
#==============================================================================

run: $(BENCHMARK)
	@echo "Running benchmark..."
	$(BENCHMARK)

#==============================================================================
# Cleanup
#==============================================================================

clean:
	@echo "Cleaning benchmark artifacts..."
	rm -rf $(GENERATED_SOURCES) *.h
	rm -rf $(BENCH_BUILD)
