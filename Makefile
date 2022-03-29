.PHONY: all libs clean example objs doxygen benchmark install test
.ONESHELL:

TARGET_DIR ?=

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

CFLAGS += -Wall -Wextra -Werror -std=c11 -ggdb -Wno-unused-result -I$(ROOT_DIR)/src $(SANITIZER_FLAGS) $(DEBUG_FLAGS)
CXXFLAGS += -Wall -Wextra -Werror -std=c++17 -ggdb -Wno-unused-result -I$(ROOT_DIR)/src $(SANITIZER_FLAGS) $(DEBUG_FLAGS)
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
	cp -f $(TARGET_DIR)/json-gen-c /usr/bin/

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

