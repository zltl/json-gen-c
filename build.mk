# Build Configuration for json-gen-c
# This file contains common build settings and functions

# Project structure
ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ifneq ($(findstring /test,$(ROOT_DIR)),)
    ROOT_DIR := $(shell dirname $(ROOT_DIR))
endif
ifneq ($(findstring /example,$(ROOT_DIR)),)
    ROOT_DIR := $(shell dirname $(ROOT_DIR))
endif  
ifneq ($(findstring /benchmark,$(ROOT_DIR)),)
    ROOT_DIR := $(shell dirname $(ROOT_DIR))
endif
BUILD_DIR ?= $(ROOT_DIR)/build
TARGET_DIR ?= $(BUILD_DIR)

# Create build directories
$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(BUILD_DIR)/lib)
$(shell mkdir -p $(BUILD_DIR)/bin)
$(shell mkdir -p $(BUILD_DIR)/obj)
$(shell mkdir -p $(BUILD_DIR)/test)

# Compiler settings
CC ?= gcc
CXX ?= g++
AR ?= ar

# Debug and sanitizer flags
DEBUG_FLAGS :=
SANITIZER_FLAGS :=

ifneq ($(JSON_DEBUG),)
	DEBUG_FLAGS := -DJSON_DEBUG
endif

ifneq ($(JSON_SANITIZE),)
	SANITIZER_FLAGS := -fsanitize=address -lasan
endif

# Common compilation flags
COMMON_FLAGS := -Wall -Wextra -Werror -ggdb -Wno-unused-result \
	-I$(ROOT_DIR)/src \
	$(SANITIZER_FLAGS) $(DEBUG_FLAGS)

CFLAGS := -std=c11 $(COMMON_FLAGS) $(CFLAGS)
CXXFLAGS := -std=c++17 $(COMMON_FLAGS) $(CXXFLAGS)

# Library paths
UTILS_LIB := $(BUILD_DIR)/lib/libutils.a
STRUCT_LIB := $(BUILD_DIR)/lib/libstruct.a
GENCODE_LIB := $(BUILD_DIR)/lib/libgencode.a

# All libraries
ALL_LIBS := $(UTILS_LIB) $(STRUCT_LIB) $(GENCODE_LIB)

# Main executable
JSON_GEN_C := $(BUILD_DIR)/bin/json-gen-c

# Function to compile C source files
# Usage: $(call compile-c,source,object,extra_flags)
define compile-c
$(2): $(1)
	@echo "Compiling C: $$<"
	@mkdir -p $$(dir $(2))
	$(CC) $(CFLAGS) $(3) -c $$< -o $$@
endef

# Function to compile C++ source files  
# Usage: $(call compile-cxx,source,object,extra_flags)
define compile-cxx
$(2): $(1)
	@echo "Compiling C++: $$<"
	@mkdir -p $$(dir $(2))
	$(CXX) $(CXXFLAGS) $(3) -c $$< -o $$@
endef

# Function to create static library
# Usage: $(call create-lib,libname,objects)
define create-lib
$(1): $(2)
	@echo "Creating library: $$@"
	@mkdir -p $$(dir $(1))
	$(AR) rcs $$@ $$^
endef

# Function to link executable
# Usage: $(call link-exe,target,objects,libs,extra_flags)
define link-exe
$(1): $(2) $(3)
	@echo "Linking executable: $$@"
	@mkdir -p $$(dir $(1))
	$(CC) $(CFLAGS) $(2) $(3) $(4) -o $$@
endef

# Function to link C++ executable
# Usage: $(call link-cxx-exe,target,objects,libs,extra_flags)
define link-cxx-exe
$(1): $(2) $(3)
	@echo "Linking C++ executable: $$@"
	@mkdir -p $$(dir $(1))
	$(CXX) $(CXXFLAGS) $(2) $(3) $(4) -o $$@
endef

# Export all variables
export
