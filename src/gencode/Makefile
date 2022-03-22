sub_name = gencode

.PHONY: all inc

sources_c = $(wildcard *.c)
objs_c = $(patsubst %.c,$(TARGET_DIR)/$(sub_name)/%.c.o,$(sources_c))
$(shell mkdir -p $(TARGET_DIR)/$(sub_name))

all: inc $(TARGET_DIR)/$(sub_name).a

inc: $(TARGET_DIR)/$(sub_name)/extra_codes.inc

$(TARGET_DIR)/$(sub_name)/extra_codes.inc: codes/json_parse.c codes/json_parse.h
	xxd -i codes/json_parse.c >$(TARGET_DIR)/$(sub_name)/extra_codes.inc
	xxd -i codes/json_parse.h >>$(TARGET_DIR)/$(sub_name)/extra_codes.inc

$(TARGET_DIR)/$(sub_name)/%.c.o: %.c
	$(CC) $(CFLAGS) -I$(TARGET_DIR)/$(sub_name)/ -c $< -o $@
$(TARGET_DIR)/$(sub_name).a: $(objs_c)
	$(AR) rcs $@ $^
