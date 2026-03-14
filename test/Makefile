
# Test Makefile for json-gen-c
include ../build.mk

.PHONY: all clean run

# Test build directory
TEST_BUILD := $(BUILD_DIR)/test

# Test source files
TEST_SOURCES := simple_test.cc struct_test.cc enhanced_test.cc empty_array_bugfix_test.cc comprehensive_test.cc nested_struct_test.cc performance_test.cc hash_map_test.cc enum_test.cc fixed_array_test.cc map_test.cc optional_test.cc precise_int_test.cc diagnostic_test.cc alias_test.cc
TEST_OBJECTS := $(patsubst %.cc,$(TEST_BUILD)/%.o,$(TEST_SOURCES))

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(TEST_BUILD)/%.o,$(GENERATED_SOURCES))

# Test executables
UNIT_TEST := $(TEST_BUILD)/unit_test
ENHANCED_TEST := $(TEST_BUILD)/enhanced_test
EMPTY_ARRAY_TEST := $(TEST_BUILD)/empty_array_test
COMPREHENSIVE_TEST := $(TEST_BUILD)/comprehensive_test
NESTED_STRUCT_TEST := $(TEST_BUILD)/nested_struct_test
PERFORMANCE_TEST := $(TEST_BUILD)/performance_test
HASH_MAP_TEST := $(TEST_BUILD)/hash_map_test
ENUM_TEST := $(TEST_BUILD)/enum_test
FIXED_ARRAY_TEST := $(TEST_BUILD)/fixed_array_test
MAP_TEST := $(TEST_BUILD)/map_test
OPTIONAL_TEST := $(TEST_BUILD)/optional_test
PRECISE_INT_TEST := $(TEST_BUILD)/precise_int_test
DIAGNOSTIC_TEST := $(TEST_BUILD)/diagnostic_test
ALIAS_TEST := $(TEST_BUILD)/alias_test

# All test targets
ALL_TESTS := $(UNIT_TEST) $(ENHANCED_TEST) $(EMPTY_ARRAY_TEST) $(COMPREHENSIVE_TEST) $(NESTED_STRUCT_TEST) $(PERFORMANCE_TEST) $(HASH_MAP_TEST) $(ENUM_TEST) $(FIXED_ARRAY_TEST) $(MAP_TEST) $(OPTIONAL_TEST) $(PRECISE_INT_TEST) $(DIAGNOSTIC_TEST) $(ALIAS_TEST)

#==============================================================================
# Build rules
#==============================================================================

all: $(ALL_TESTS)

# Generate test structures first
json.gen.c sstr.c: test.json-gen-c $(JSON_GEN_C)
	@echo "Generating test structures..."
	$(JSON_GEN_C) -in test.json-gen-c -out .

# Compile generated C files
$(foreach src,$(GENERATED_SOURCES),$(eval $(call compile-c,$(src),$(patsubst %.c,$(TEST_BUILD)/%.o,$(src)),-I. -I$(ROOT_DIR)/src -I$(ROOT_DIR)/src/utils)))

# Compile test C++ files
$(foreach src,$(TEST_SOURCES),$(eval $(call compile-cxx,$(src),$(patsubst %.cc,$(TEST_BUILD)/%.o,$(src)),-I.)))

# Make test objects depend on generated files
$(TEST_OBJECTS): json.gen.c

# Link test executables
$(UNIT_TEST): $(TEST_BUILD)/simple_test.o $(TEST_BUILD)/struct_test.o $(GENERATED_OBJECTS)
	@echo "Linking unit tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(ENHANCED_TEST): $(TEST_BUILD)/enhanced_test.o
	@echo "Linking enhanced tests: $@" 
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread -L$(BUILD_DIR)/lib -lutils

$(EMPTY_ARRAY_TEST): $(TEST_BUILD)/empty_array_bugfix_test.o $(GENERATED_OBJECTS)
	@echo "Linking empty array tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(COMPREHENSIVE_TEST): $(TEST_BUILD)/comprehensive_test.o $(GENERATED_OBJECTS)
	@echo "Linking comprehensive tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(NESTED_STRUCT_TEST): $(TEST_BUILD)/nested_struct_test.o $(GENERATED_OBJECTS)
	@echo "Linking nested struct tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(PERFORMANCE_TEST): $(TEST_BUILD)/performance_test.o $(GENERATED_OBJECTS)
	@echo "Linking performance tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(HASH_MAP_TEST): $(TEST_BUILD)/hash_map_test.o $(GENERATED_OBJECTS)
	@echo "Linking hash map tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread -L$(BUILD_DIR)/lib -lutils

$(ENUM_TEST): $(TEST_BUILD)/enum_test.o $(GENERATED_OBJECTS)
	@echo "Linking enum tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(FIXED_ARRAY_TEST): $(TEST_BUILD)/fixed_array_test.o $(GENERATED_OBJECTS)
	@echo "Linking fixed array tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(MAP_TEST): $(TEST_BUILD)/map_test.o $(GENERATED_OBJECTS)
	@echo "Linking map tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(OPTIONAL_TEST): $(TEST_BUILD)/optional_test.o $(GENERATED_OBJECTS)
	@echo "Linking optional tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(PRECISE_INT_TEST): $(TEST_BUILD)/precise_int_test.o $(GENERATED_OBJECTS)
	@echo "Linking precise int tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(DIAGNOSTIC_TEST): $(TEST_BUILD)/diagnostic_test.o
	@echo "Linking diagnostic tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread -L$(BUILD_DIR)/lib -lstruct -lutils

$(ALIAS_TEST): $(TEST_BUILD)/alias_test.o $(GENERATED_OBJECTS)
	@echo "Linking alias tests: $@"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

#==============================================================================
# Test execution
#==============================================================================

run: $(ALL_TESTS)
	@echo "Running all tests..."
	@echo "=== Unit Tests ==="
	$(UNIT_TEST)
	@echo "=== Enhanced Tests ==="
	$(ENHANCED_TEST)
	@echo "=== Empty Array Tests ==="
	$(EMPTY_ARRAY_TEST)
	@echo "=== Comprehensive Tests ==="
	$(COMPREHENSIVE_TEST)
	@echo "=== Nested Struct Tests ==="
	$(NESTED_STRUCT_TEST)
	@echo "=== Performance Tests ==="
	$(PERFORMANCE_TEST)
	@echo "=== Hash Map Tests ==="
	$(HASH_MAP_TEST)
	@echo "=== Enum Tests ==="
	$(ENUM_TEST)
	@echo "=== Fixed Array Tests ==="
	$(FIXED_ARRAY_TEST)
	@echo "=== Map Tests ==="
	$(MAP_TEST)
	@echo "=== Optional Tests ==="
	$(OPTIONAL_TEST)
	@echo "=== Precise Int Tests ==="
	$(PRECISE_INT_TEST)
	@echo "=== Diagnostic Tests ==="
	$(DIAGNOSTIC_TEST)
	@echo "=== Alias Tests ==="
	$(ALIAS_TEST)
	@echo "=== Bugfix Tests ==="
	$(MAKE) run-bugfix
	@echo "All tests completed successfully!"

# Individual test runs
run-bugfix: $(JSON_GEN_C)
	@echo "Running bugfix test (off-by-one)..."
	@mkdir -p $(TEST_BUILD)/bugfix_out
	$(JSON_GEN_C) -in bugfix_off_by_one.json-gen-c -out $(TEST_BUILD)/bugfix_out
	@rm -rf $(TEST_BUILD)/bugfix_out
	@echo "Running bugfix test (include before struct)..."
	@mkdir -p $(TEST_BUILD)/bugfix_include_out
	$(JSON_GEN_C) -in bugfix_include_before_struct.json-gen-c -out $(TEST_BUILD)/bugfix_include_out
	@rm -rf $(TEST_BUILD)/bugfix_include_out
	@echo "Running bugfix test (empty enum)..."
	@mkdir -p $(TEST_BUILD)/bugfix_empty_enum_out
	$(JSON_GEN_C) -in bugfix_empty_enum.json-gen-c -out $(TEST_BUILD)/bugfix_empty_enum_out
	@rm -rf $(TEST_BUILD)/bugfix_empty_enum_out
	@echo "Running bugfix test (include enum cross-file)..."
	@mkdir -p $(TEST_BUILD)/bugfix_include_enum_out
	$(JSON_GEN_C) -in bugfix_include_enum.json-gen-c -out $(TEST_BUILD)/bugfix_include_enum_out
	@rm -rf $(TEST_BUILD)/bugfix_include_enum_out
	@echo "Running negative tests (malformed schemas should fail)..."
	@mkdir -p $(TEST_BUILD)/malformed_out
	@! $(JSON_GEN_C) -in malformed_enum_no_brace.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_enum_no_brace should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_enum_no_name.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_enum_no_name should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_unexpected_token.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_unexpected_token should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_undefined_enum_ref.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_undefined_enum_ref should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_duplicate_struct.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_duplicate_struct should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_duplicate_enum.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_duplicate_enum should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_duplicate_field.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_duplicate_field should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_duplicate_enum_value.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_duplicate_enum_value should have failed" && false)
	@! $(JSON_GEN_C) -in malformed_struct_enum_name_clash.json-gen-c -out $(TEST_BUILD)/malformed_out 2>/dev/null \
		|| (echo "FAIL: malformed_struct_enum_name_clash should have failed" && false)
	@rm -rf $(TEST_BUILD)/malformed_out

run-unit: $(UNIT_TEST)
	@echo "Running unit tests..."
	$(UNIT_TEST)

run-enhanced: $(ENHANCED_TEST)
	@echo "Running enhanced tests..."
	$(ENHANCED_TEST)
	
run-empty-array: $(EMPTY_ARRAY_TEST)
	@echo "Running empty array tests..."
	$(EMPTY_ARRAY_TEST)

run-comprehensive: $(COMPREHENSIVE_TEST)
	@echo "Running comprehensive tests..."
	$(COMPREHENSIVE_TEST)

run-nested: $(NESTED_STRUCT_TEST)
	@echo "Running nested struct tests..."
	$(NESTED_STRUCT_TEST)

run-performance: $(PERFORMANCE_TEST)
	@echo "Running performance tests..."
	$(PERFORMANCE_TEST)

run-diagnostic: $(DIAGNOSTIC_TEST)
	@echo "Running diagnostic tests..."
	$(DIAGNOSTIC_TEST)

#==============================================================================
# Cleanup
#==============================================================================

clean:
	@echo "Cleaning test artifacts..."
	rm -rf $(GENERATED_SOURCES) *.h
	rm -rf $(TEST_BUILD)

.PHONY: run run-unit run-enhanced run-empty-array run-comprehensive run-nested run-performance run-diagnostic