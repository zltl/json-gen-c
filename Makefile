.PHONY: all libs clean example objs doxygen benchmark install test
.ONESHELL:

TARGET_DIR ?=

DEST ?= /usr/

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ifeq ($(TARGET_DIR),)
	TARGET_DIR = $(ROOT_DIR)/target
endif
$(shell mkdir -p $(TARGET_DIR))

ifneq ($(JSON_DEBUG),)
	DEBUG_FLAGS = -DJSON_DEBUG
	SANITIZER_FLAGS = -fsanitize=address -lasan
endif

ifneq ($(JSON_SANITIZE),)
	SANITIZER_FLAGS = -fsanitize=address -lasan
endif

COMMON_FLAGS += -Wall -Wextra -Werror -ggdb -Wno-unused-result \
	-I$(ROOT_DIR)/src \
	-I/usr/include \
	$(SANITIZER_FLAGS) $(DEBUG_FLAGS) 

CFLAGS += -std=c11 $(COMMON_FLAGS)
CXXFLAGS += -std=c++17 $(COMMON_FLAGS)
LDFLAGS ?=

export

all: json-gen-c

libs: $(TARGET_DIR)/utils.a $(TARGET_DIR)/struct.a $(TARGET_DIR)/gencode.a

$(TARGET_DIR)/utils.a: $(wildcard src/utils/*.c) $(wildcard src/utils/*.h)
	make -C src/utils
$(TARGET_DIR)/struct.a: $(wildcard src/struct/*.c) $(wildcard src/struct/*.h)
	make -C src/struct
$(TARGET_DIR)/gencode.a: $(wildcard src/gencode/*.c) $(wildcard src/gencode/*.h) $(wildcard src/gencode/codes/*)
	make -C src/gencode

libs_mmm = $(TARGET_DIR)/gencode.a $(TARGET_DIR)/struct.a $(TARGET_DIR)/utils.a

$(TARGET_DIR)/main.o: src/main/main.c
	make -C src/main
	$(CC) $(CFLAGS) -I$(TARGET_DIR) -c $< -o $@

json-gen-c: $(TARGET_DIR)/json-gen-c

$(TARGET_DIR)/json-gen-c: libs $(TARGET_DIR)/main.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/main.o $(libs_mmm) -o $@

install: $(TARGET_DIR)/json-gen-c
	@cp -f $(TARGET_DIR)/json-gen-c $(DEST)/bin/json-gen-c
	@cp -f $(ROOT_DIR)/doc/json-gen-c.1 $(DEST)/share/man/man1/
	@gzip -f $(DEST)/share/man/man1/json-gen-c.1
	@echo json-gen-c has been installed on your device

uninstall:
	@rm -rf $(DEST)/bin/json-gen-c
	@rm -rf $(DEST)/share/man/man1/json-gen-c.1.gz
	@echo json-gen-c has been removed from your device

clean:
	rm -rf $(TARGET_DIR)
	make clean -C example
	make clean -C benchmark
	make clean -C test

doxygen:
	if [ ! -d target/doxygen-awesome-css ]; then
		mkdir -p target
		git clone --depth=1 https://github.com/jothepro/doxygen-awesome-css.git target/doxygen-awesome-css
	fi
	doxygen doc/Doxyfile

example: json-gen-c
	make -C example

benchmark: json-gen-c
	make -C benchmark

test: json-gen-c
	make -C test

