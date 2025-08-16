# Benchmark Makefile for json-gen-c
include ../build.mk

.PHONY: all clean run

# Benchmark build directory
BENCH_BUILD := $(BUILD_DIR)/benchmark

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(BENCH_BUILD)/%.o,$(GENERATED_SOURCES))

# Benchmark source and object
BENCH_SOURCE := json_bench.cc
BENCH_OBJECT := $(BENCH_BUILD)/json_bench.o

# Benchmark executable
BENCHMARK := $(BENCH_BUILD)/json_bench

#==============================================================================
# Build rules
#==============================================================================

all: $(BENCHMARK)

# Generate benchmark structures
json.gen.c sstr.c: structs.json-gen-c $(JSON_GEN_C)
	@echo "Generating benchmark structures..."
	$(JSON_GEN_C) -in structs.json-gen-c -out .

# Compile generated files
$(foreach src,$(GENERATED_SOURCES),$(eval $(call compile-c,$(src),$(patsubst %.c,$(BENCH_BUILD)/%.o,$(src)),-I.)))

# Compile benchmark main
$(eval $(call compile-cxx,$(BENCH_SOURCE),$(BENCH_OBJECT),-I.))

# Make benchmark object depend on generated files
$(BENCH_OBJECT): json.gen.c

# Link benchmark executable
$(BENCHMARK): $(BENCH_OBJECT) $(GENERATED_OBJECTS)
	@echo "Linking benchmark: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lbenchmark -lpthread

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
