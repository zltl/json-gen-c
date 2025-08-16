# Modern Makefile for json-gen-c
# Includes common build configuration
include build.mk

.PHONY: all libs clean example benchmark install uninstall test doxygen
.DEFAULT_GOAL := all

# Installation directory
DEST ?= /usr/

#==============================================================================
# Source file definitions
#==============================================================================

# Utils library sources
UTILS_SOURCES := $(wildcard src/utils/*.c)
UTILS_OBJECTS := $(patsubst src/utils/%.c,$(BUILD_DIR)/obj/utils/%.o,$(UTILS_SOURCES))

# Struct library sources  
STRUCT_SOURCES := $(wildcard src/struct/*.c)
STRUCT_OBJECTS := $(patsubst src/struct/%.c,$(BUILD_DIR)/obj/struct/%.o,$(STRUCT_SOURCES))

# Gencode library sources
GENCODE_SOURCES := $(wildcard src/gencode/*.c)
GENCODE_OBJECTS := $(patsubst src/gencode/%.c,$(BUILD_DIR)/obj/gencode/%.o,$(GENCODE_SOURCES))

# Main executable sources
MAIN_SOURCES := src/main/main.c
MAIN_OBJECTS := $(patsubst src/main/%.c,$(BUILD_DIR)/obj/main/%.o,$(MAIN_SOURCES))

# Extra code generation for gencode
GENCODE_EXTRA := $(BUILD_DIR)/obj/gencode/extra_codes.inc
MAIN_EXTRA := $(BUILD_DIR)/obj/main/extra_codes_sstr.inc

#==============================================================================
# Build rules
#==============================================================================

all: $(JSON_GEN_C)

# Libraries
libs: $(ALL_LIBS)

# Generate compilation rules for all source files
$(foreach src,$(UTILS_SOURCES),$(eval $(call compile-c,$(src),$(patsubst src/utils/%.c,$(BUILD_DIR)/obj/utils/%.o,$(src)))))
$(foreach src,$(STRUCT_SOURCES),$(eval $(call compile-c,$(src),$(patsubst src/struct/%.c,$(BUILD_DIR)/obj/struct/%.o,$(src)))))

# Special rule for main (needs extra include)
$(foreach src,$(MAIN_SOURCES),$(eval $(call compile-c,$(src),$(patsubst src/main/%.c,$(BUILD_DIR)/obj/main/%.o,$(src)),-I$(BUILD_DIR)/obj/main)))

# Special rule for gencode (needs extra include)
$(foreach src,$(GENCODE_SOURCES),$(eval $(call compile-c,$(src),$(patsubst src/gencode/%.c,$(BUILD_DIR)/obj/gencode/%.o,$(src)),-I$(BUILD_DIR)/obj/gencode)))

# Generate extra codes for gencode
$(GENCODE_EXTRA): src/gencode/codes/json_parse.c src/gencode/codes/json_parse.h
	@echo "Generating extra codes: $@"
	@mkdir -p $(dir $@)
	cd src/gencode/codes && xxd -i json_parse.c > $(abspath $@)
	cd src/gencode/codes && xxd -i json_parse.h >> $(abspath $@)

# Generate extra codes for main (sstr utilities)
$(MAIN_EXTRA): src/utils/sstr.c src/utils/sstr.h
	@echo "Generating sstr extra codes: $@"
	@mkdir -p $(dir $@)
	cd src/utils && xxd -i sstr.c > $(abspath $@)
	cd src/utils && xxd -i sstr.h >> $(abspath $@)

# Create libraries
$(eval $(call create-lib,$(UTILS_LIB),$(UTILS_OBJECTS)))
$(eval $(call create-lib,$(STRUCT_LIB),$(STRUCT_OBJECTS)))
$(eval $(call create-lib,$(GENCODE_LIB),$(GENCODE_OBJECTS)))

# Gencode library depends on extra codes
$(GENCODE_OBJECTS): $(GENCODE_EXTRA)

# Main objects depend on extra codes
$(MAIN_OBJECTS): $(MAIN_EXTRA)

# Main executable
$(eval $(call link-exe,$(JSON_GEN_C),$(MAIN_OBJECTS),$(GENCODE_LIB) $(STRUCT_LIB) $(UTILS_LIB)))

#==============================================================================
# Subdirectory targets
#==============================================================================

example: $(JSON_GEN_C)
	@echo "Building example..."
	$(MAKE) -C example

benchmark: $(JSON_GEN_C)
	@echo "Building benchmark..."
	$(MAKE) -C benchmark

test: $(JSON_GEN_C)
	@echo "Building and running tests..."
	$(MAKE) -C test

#==============================================================================
# Installation and cleanup
#==============================================================================

install: $(JSON_GEN_C)
	@echo "Installing json-gen-c..."
	@mkdir -p $(DEST)/bin $(DEST)/share/man/man1
	@cp -f $(JSON_GEN_C) $(DEST)/bin/json-gen-c
	@cp -f doc/json-gen-c.1 $(DEST)/share/man/man1/
	@gzip -f $(DEST)/share/man/man1/json-gen-c.1
	@echo "json-gen-c has been installed to $(DEST)"

uninstall:
	@echo "Uninstalling json-gen-c..."
	@rm -f $(DEST)/bin/json-gen-c
	@rm -f $(DEST)/share/man/man1/json-gen-c.1.gz
	@echo "json-gen-c has been removed from $(DEST)"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	$(MAKE) clean -C example
	$(MAKE) clean -C benchmark  
	$(MAKE) clean -C test

#==============================================================================
# Documentation
#==============================================================================

doxygen:
	@echo "Generating documentation..."
	@if [ ! -d $(BUILD_DIR)/doxygen-awesome-css ]; then \
		mkdir -p $(BUILD_DIR); \
		git clone --depth=1 https://github.com/jothepro/doxygen-awesome-css.git $(BUILD_DIR)/doxygen-awesome-css; \
	fi
	doxygen doc/Doxyfile

#==============================================================================
# Debug targets
#==============================================================================

debug: JSON_DEBUG=1
debug: all

sanitize: JSON_SANITIZE=1  
sanitize: all

# Show build configuration
show-config:
	@echo "Build Configuration:"
	@echo "  ROOT_DIR: $(ROOT_DIR)"
	@echo "  BUILD_DIR: $(BUILD_DIR)"
	@echo "  CC: $(CC)"
	@echo "  CXX: $(CXX)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  CXXFLAGS: $(CXXFLAGS)"
	@echo "  JSON_DEBUG: $(JSON_DEBUG)"
	@echo "  JSON_SANITIZE: $(JSON_SANITIZE)"

.PHONY: debug sanitize show-config

