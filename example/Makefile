# Example Makefile for json-gen-c
include ../build.mk

.PHONY: all clean run

# Example build directory
EXAMPLE_BUILD := $(BUILD_DIR)/example

# Generated files
GENERATED_SOURCES := json.gen.c sstr.c
GENERATED_OBJECTS := $(patsubst %.c,$(EXAMPLE_BUILD)/%.o,$(GENERATED_SOURCES))

# Example source and object
EXAMPLE_SOURCE := example_main.c
EXAMPLE_OBJECT := $(EXAMPLE_BUILD)/example_main.o

# Example executable
EXAMPLE := $(EXAMPLE_BUILD)/example

#==============================================================================
# Build rules
#==============================================================================

all: $(EXAMPLE)

# Generate example structures
json.gen.c sstr.c: example.json-gen-c $(JSON_GEN_C)
	@echo "Generating example structures..."
	$(JSON_GEN_C) -in example.json-gen-c -out .

# Compile generated files
$(foreach src,$(GENERATED_SOURCES),$(eval $(call compile-c,$(src),$(patsubst %.c,$(EXAMPLE_BUILD)/%.o,$(src)),-I. -I$(ROOT_DIR)/src)))

# Compile example main
$(eval $(call compile-c,$(EXAMPLE_SOURCE),$(EXAMPLE_OBJECT),-I.))

# Make example main depend on generated files
$(EXAMPLE_OBJECT): json.gen.c

# Link example executable
$(EXAMPLE): $(EXAMPLE_OBJECT) $(GENERATED_OBJECTS)
	@echo "Linking example: $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

#==============================================================================
# Run example
#==============================================================================

run: $(EXAMPLE)
	@echo "Running example..."
	$(EXAMPLE)

#==============================================================================
# Cleanup
#==============================================================================

clean:
	@echo "Cleaning example artifacts..."
	rm -rf $(GENERATED_SOURCES) *.h
	rm -rf $(EXAMPLE_BUILD)
