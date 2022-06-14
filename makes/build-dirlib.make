CURR_DIR=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
sub_name = lib$(shell basename $(CURR_DIR))

sources_c = $(wildcard *.c)
objs_c = $(patsubst %.c,$(TARGET_DIR)/$(sub_name)/%.c.o,$(sources_c))
$(shell mkdir -p $(TARGET_DIR)/$(sub_name))

all: $(TARGET_DIR)/$(sub_name).a

$(TARGET_DIR)/$(sub_name)/%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
$(TARGET_DIR)/$(sub_name).a: $(objs_c)
	$(AR) rcs $@ $^
