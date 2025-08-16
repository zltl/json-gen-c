
# Test Makefile for json-gen-c
include ../build.mk

.PHONY: all clean run

# Test build directory
TEST_BUILD := $(BUILD_DIR)/test

# Test source files
TEST_SOURCES := simple_test.cc struct_test.cc enhanced_test.cc empty_array_bugfix_test.cc
TEST_OBJECTS := $(patsubst %.cc,$(TEST_BUILD)/%.o,$(TEST_SOURCES))

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(TEST_BUILD)/%.o,$(GENERATED_SOURCES))

# Test executables
UNIT_TEST := $(TEST_BUILD)/unit_test
ENHANCED_TEST := $(TEST_BUILD)/enhanced_test
EMPTY_ARRAY_TEST := $(TEST_BUILD)/empty_array_test

# All test targets
ALL_TESTS := $(UNIT_TEST) $(ENHANCED_TEST) $(EMPTY_ARRAY_TEST)

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
	@echo "All tests completed successfully!"

# Individual test runs
run-unit: $(UNIT_TEST)
	@echo "Running unit tests..."
	$(UNIT_TEST)

run-enhanced: $(ENHANCED_TEST)
	@echo "Running enhanced tests..."
	$(ENHANCED_TEST)
	
run-empty-array: $(EMPTY_ARRAY_TEST)
	@echo "Running empty array tests..."
	$(EMPTY_ARRAY_TEST)

#==============================================================================
# Cleanup
#==============================================================================

clean:
	@echo "Cleaning test artifacts..."
	rm -rf $(GENERATED_SOURCES) *.h
	rm -rf $(TEST_BUILD)

.PHONY: run run-unit run-enhanced run-empty-array