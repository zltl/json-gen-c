.PHONY: all clean

all: $(TARGET_DIR)/example

$(TARGET_DIR)/example: example.json-gen-c example_main.c
	$(TARGET_DIR)/json-gen-c -in example.json-gen-c -out .
	$(CC) $(CFLAGS) json.gen.c example_main.c -I. sstr.c -o $@
	
clean:
	rm -rf json.gen* sstr*
