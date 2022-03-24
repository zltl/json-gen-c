#include <benchmark/benchmark.h>

#include "json.gen.h"

static void BM_marshal_scalar(benchmark::State& state) {
    struct scalar obj;
    scalar_init(&obj);
    obj.int_val1 = 1;
    obj.int_val2 = 1;
    obj.long_val = 2;
    obj.double_val = 3.0;
    obj.float_val = 4.0;
    obj.sstr_val = sstr("hello this is string");
    sstr_t out = sstr_new();
    size_t total = 0;
    for (auto _ : state) {
        // This code gets timed
        json_marshal_scalar(&obj, out);
        total += sstr_length(out);
        sstr_clear(out);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(out);
    scalar_clear(&obj);
}

static void BM_unmarshal_scalar(benchmark::State& state) {
    struct scalar obj;
    scalar_init(&obj);
    obj.int_val1 = 1;
    obj.int_val2 = 1;
    obj.long_val = 2;
    obj.double_val = 3.0;
    obj.float_val = 4.0;
    obj.sstr_val = sstr("hello this is string");
    sstr_t content = sstr_new();
    json_marshal_scalar(&obj, content);
    size_t total = 0;
    for (auto _ : state) {
        scalar_clear(&obj);
        json_unmarshal_scalar(content, &obj);
        total += sstr_length(content);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
    scalar_clear(&obj);
}

static void BM_marshal_scalar_array(benchmark::State& state) {
    int len = (int)state.range(0);
    struct scalar* obj = new struct scalar[len];
    for (int i = 0; i < len; i++) {
        scalar_init(&obj[i]);
        obj[i].int_val1 = 1;
        obj[i].int_val2 = 1;
        obj[i].long_val = 2;
        obj[i].double_val = 3.0;
        obj[i].float_val = 4.0;
        obj[i].sstr_val = sstr("hello this is string");
    }
    sstr_t out = sstr_new();
    size_t total = 0;
    for (auto _ : state) {
        // This code gets timed
        json_marshal_array_scalar(obj, len, out);
        total += sstr_length(out);
        sstr_clear(out);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(out);
    for (int i = 0; i < len; i++) {
        scalar_clear(&obj[i]);
    }
    delete[] obj;
}

static void BM_unmarshal_scalar_array(benchmark::State& state) {
    int len = (int)state.range(0);
    struct scalar* obj = new struct scalar[len];
    for (int i = 0; i < len; i++) {
        scalar_init(&obj[i]);
        obj[i].int_val1 = 1;
        obj[i].int_val2 = 1;
        obj[i].long_val = 2;
        obj[i].double_val = 3.0;
        obj[i].float_val = 4.0;
        obj[i].sstr_val = sstr("hello this is string");
    }
    sstr_t content = sstr_new();
    json_marshal_array_scalar(obj, len, content);
    for (int i = 0; i < len; i++) {
        scalar_clear(&obj[i]);
    }
    delete[] obj;

    size_t total = 0;
    for (auto _ : state) {
        // This code gets timed
        struct scalar* robj = NULL;
        int rlen = 0;
        json_unmarshal_array_scalar(content, &robj, &rlen);
        total += sstr_length(content);
        for (int i = 0; i < rlen; i++) {
            scalar_clear(&robj[i]);
        }
        free(robj);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
}

BENCHMARK(BM_marshal_scalar);
BENCHMARK(BM_unmarshal_scalar);
BENCHMARK(BM_marshal_scalar_array)
    ->Args({8})
    ->Args({16})
    ->Args({32})
    ->Args({64});
BENCHMARK(BM_unmarshal_scalar_array)
    ->Args({8})
    ->Args({16})
    ->Args({32})
    ->Args({64});

BENCHMARK_MAIN();
