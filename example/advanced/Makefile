# Advanced example Makefile for json-gen-c
include ../../build.mk

# Override: we're two levels deep under example/advanced/
override ROOT_DIR := $(abspath $(CURDIR)/../..)
override BUILD_DIR := $(ROOT_DIR)/build
override JSON_GEN_C := $(BUILD_DIR)/bin/json-gen-c

.PHONY: all clean run

# Build directory
EXAMPLE_BUILD := $(BUILD_DIR)/example/advanced

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(EXAMPLE_BUILD)/%.o,$(GENERATED_SOURCES))

# Example source and object
EXAMPLE_SOURCE := advanced_main.c
EXAMPLE_OBJECT := $(EXAMPLE_BUILD)/advanced_main.o

# Example executable
EXAMPLE := $(EXAMPLE_BUILD)/advanced

#==============================================================================
# Build rules
#==============================================================================

all: $(EXAMPLE)

# Generate structures
json.gen.c sstr.c: advanced.json-gen-c $(JSON_GEN_C)
	@echo "Generating advanced example structures..."
	$(JSON_GEN_C) -in advanced.json-gen-c -out .

# Compile generated files
$(foreach src,$(GENERATED_SOURCES),$(eval $(call compile-c,$(src),$(patsubst %.c,$(EXAMPLE_BUILD)/%.o,$(src)),-I. -I$(ROOT_DIR)/src)))

# Compile example main
$(eval $(call compile-c,$(EXAMPLE_SOURCE),$(EXAMPLE_OBJECT),-I.))

# Make example main depend on generated files
$(EXAMPLE_OBJECT): json.gen.c

# Link example executable
$(EXAMPLE): $(EXAMPLE_OBJECT) $(GENERATED_OBJECTS)
	@echo "Linking advanced example: $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

#==============================================================================
# Run example
#==============================================================================

run: $(EXAMPLE)
	@echo "Running advanced example..."
	@echo ""
	@$(EXAMPLE)

#==============================================================================
# Cleanup
#==============================================================================

clean:
	@echo "Cleaning advanced example artifacts..."
	rm -rf $(GENERATED_SOURCES) *.h
	rm -rf $(EXAMPLE_BUILD)
