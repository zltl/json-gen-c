.PHONY: all clean

.ONESHELL:

$(shell mkdir -p $(TARGET_DIR)/benchmark)

BENCH_DIR=$(TARGET_DIR)/benchmark

all: $(BENCH_DIR)/json_bench

json.gen.c: structs.json-gen-c 
	$(TARGET_DIR)/json-gen-c -in structs.json-gen-c -out .
$(BENCH_DIR)/sstr.c.o: json.gen.c
	$(CC) $(CFLAGS) -c sstr.c -o $@
$(BENCH_DIR)/json.gen.c.o: json.gen.c structs.json-gen-c
	$(CC) $(CFLAGS) -I. -c json.gen.c -o $@
$(BENCH_DIR)/json_bench.cc.o: json_bench.cc
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

$(BENCH_DIR)/json_bench: $(BENCH_DIR)/json.gen.c.o $(BENCH_DIR)/json_bench.cc.o $(BENCH_DIR)/sstr.c.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -lbenchmark -lpthread -o $@

clean:
	rm -rf json.gen* sstr*
