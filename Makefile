.PHONY: clean example objs doxygen
.ONESHELL:

TARGET_DIR ?=

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ifeq ($(TARGET_DIR),)
	TARGET_DIR = $(ROOT_DIR)/target
endif
$(shell mkdir -p $(TARGET_DIR))
SANITIZER_FLAGS = -fsanitize=address -lasan
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -ggdb -I$(ROOT_DIR)/src $(SANITIZER_FLAGS)
LDFLAGS ?=

export

all: json-gen-c

libs: $(TARGET_DIR)/utils.a $(TARGET_DIR)/struct.a $(TARGET_DIR)/gencode.a

$(TARGET_DIR)/utils.a: $(wildcard src/utils/*.c) $(wildcard src/utils/*.h)
	make -C src/utils
$(TARGET_DIR)/struct.a: $(wildcard src/struct/*.c) $(wildcard src/struct/*.h)
	make -C src/struct
#$(TARGET_DIR)/json.a: $(wildcard src/json/*.c) $(wildcard src/json/*.h)$
#	make -C src/json
$(TARGET_DIR)/gencode.a: $(wildcard src/gencode/*.c) $(wildcard src/gencode/*.h)
	make -C src/gencode

libs_mmm = $(TARGET_DIR)/gencode.a $(TARGET_DIR)/struct.a $(TARGET_DIR)/utils.a

$(TARGET_DIR)/main.o: src/main/main.c
	$(CC) $(CFLAGS) -c $< -o $@

json-gen-c: $(TARGET_DIR)/json-gen-c

$(TARGET_DIR)/json-gen-c: libs $(TARGET_DIR)/main.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/main.o $(libs_mmm) -o $@

clean:
	rm -rf $(TARGET_DIR)

doxygen:
	if [ ! -d target/doxygen-awesome-css ]; then
		mkdir -p target
		git clone --depth=1 https://github.com/jothepro/doxygen-awesome-css.git target/doxygen-awesome-css
	fi
	doxygen doc/Doxyfile

xxx:
	gcc $(CFLAGS) example.c json_parse.c example_main.c -I. -I./src/utils src/utils/sstr.c -I./src -o example
	
