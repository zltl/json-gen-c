# Fuzz testing for json-gen-c
#
# Requirements: clang with libFuzzer support
#   make fuzz-schema   — fuzz the schema parser
#   make fuzz-json     — fuzz the JSON unmarshal path
#   make fuzz-smoke    — 10-second smoke run of both harnesses
#
# Override FUZZ_SECONDS to control smoke-test duration (default: 10).

ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT_DIR := $(shell dirname $(ROOT_DIR))
BUILD_DIR := $(ROOT_DIR)/build
FUZZ_BUILD := $(BUILD_DIR)/fuzz

JSON_GEN_C  := $(BUILD_DIR)/bin/json-gen-c

# Must use clang for libFuzzer
FUZZ_CC     ?= clang
FUZZ_CFLAGS := -std=c11 -g -O1 \
	-fsanitize=fuzzer,address \
	-fno-omit-frame-pointer \
	-I$(ROOT_DIR)/src \
	-I$(FUZZ_BUILD)

FUZZ_SECONDS ?= 10

STRUCT_LIB := $(BUILD_DIR)/lib/libstruct.a
UTILS_LIB  := $(BUILD_DIR)/lib/libutils.a

# Generated sources for JSON harness
GEN_C := $(FUZZ_BUILD)/json.gen.c
GEN_H := $(FUZZ_BUILD)/json.gen.h
SSTR_C := $(FUZZ_BUILD)/sstr.c
SSTR_H := $(FUZZ_BUILD)/sstr.h

.PHONY: all fuzz-schema fuzz-json fuzz-smoke clean generate

all: $(FUZZ_BUILD)/fuzz_schema_parser $(FUZZ_BUILD)/fuzz_json_unmarshal

# --- Code generation for JSON harness ---
generate: $(GEN_C)

$(GEN_C): fuzz.json-gen-c $(JSON_GEN_C) | $(FUZZ_BUILD)
	$(JSON_GEN_C) -in $< -out $(FUZZ_BUILD)

$(FUZZ_BUILD):
	@mkdir -p $@

# --- Schema parser fuzzer ---
$(FUZZ_BUILD)/fuzz_schema_parser: fuzz_schema_parser.c $(STRUCT_LIB) $(UTILS_LIB) | $(FUZZ_BUILD)
	$(FUZZ_CC) $(FUZZ_CFLAGS) $< \
		$(STRUCT_LIB) $(UTILS_LIB) \
		-o $@

# --- JSON unmarshal fuzzer ---
$(FUZZ_BUILD)/fuzz_json_unmarshal: fuzz_json_unmarshal.c $(GEN_C) $(SSTR_C) | $(FUZZ_BUILD)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -I$(FUZZ_BUILD) \
		$< $(GEN_C) $(SSTR_C) \
		-o $@

# --- Run targets ---
fuzz-schema: $(FUZZ_BUILD)/fuzz_schema_parser
	@echo "=== Fuzzing schema parser ==="
	$< corpus/schema/

fuzz-json: $(FUZZ_BUILD)/fuzz_json_unmarshal
	@echo "=== Fuzzing JSON unmarshal ==="
	$< corpus/json/

fuzz-smoke: $(FUZZ_BUILD)/fuzz_schema_parser $(FUZZ_BUILD)/fuzz_json_unmarshal
	@echo "=== Smoke test: schema parser ($(FUZZ_SECONDS)s) ==="
	@ASAN_OPTIONS=detect_leaks=0 \
	UBSAN_OPTIONS=halt_on_error=0:print_stacktrace=0 \
	$(FUZZ_BUILD)/fuzz_schema_parser corpus/schema/ \
		-max_total_time=$(FUZZ_SECONDS) -print_final_stats=1 \
		>/dev/null 2>&1 && echo "  PASSED" || echo "  FAILED"
	@echo "=== Smoke test: JSON unmarshal ($(FUZZ_SECONDS)s) ==="
	@ASAN_OPTIONS=detect_leaks=0 \
	UBSAN_OPTIONS=halt_on_error=0:print_stacktrace=0 \
	$(FUZZ_BUILD)/fuzz_json_unmarshal corpus/json/ \
		-max_total_time=$(FUZZ_SECONDS) -print_final_stats=1 \
		>/dev/null 2>&1 && echo "  PASSED" || echo "  FAILED"

clean:
	rm -rf $(FUZZ_BUILD)
