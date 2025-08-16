
.PHONY: all clean

.ONESHELL:

$(shell mkdir -p $(TARGET_DIR)/test)

TEST_DIR=$(TARGET_DIR)/test

all: $(TEST_DIR)/unit_test $(TEST_DIR)/enhanced_test $(TEST_DIR)/empty_array_test

# Generate test structures
json.gen.c: test.json-gen-c
	$(TARGET_DIR)/json-gen-c -in test.json-gen-c -out .
$(TEST_DIR)/sstr.c.o: json.gen.c
	$(CC) $(CFLAGS) -c sstr.c -o $@
$(TEST_DIR)/json.gen.c.o: json.gen.c
	$(CC) $(CFLAGS) -I. -I$(ROOT_DIR)/src -I$(ROOT_DIR)/src/utils -c json.gen.c -o $@

$(TEST_DIR)/simple_test.cc.o: simple_test.cc
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@ 
$(TEST_DIR)/struct_test.cc.o: struct_test.cc
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@
$(TEST_DIR)/enhanced_test.cc.o: enhanced_test.cc
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@
$(TEST_DIR)/empty_array_bugfix_test.cc.o: empty_array_bugfix_test.cc
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

common_objs = $(TEST_DIR)/sstr.c.o $(TEST_DIR)/json.gen.c.o

test_objs = $(TEST_DIR)/simple_test.cc.o $(TEST_DIR)/struct_test.cc.o

enhanced_test_objs = $(TEST_DIR)/enhanced_test.cc.o

empty_array_test_objs = $(TEST_DIR)/empty_array_bugfix_test.cc.o

$(TEST_DIR)/unit_test: $(common_objs) $(test_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

$(TEST_DIR)/enhanced_test: $(enhanced_test_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread -L$(ROOT_DIR)/target -lutils

$(TEST_DIR)/empty_array_test: $(common_objs) $(empty_array_test_objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ -lgtest -lgtest_main -lpthread

clean:
	rm -rf json.gen* sstr*