/**
 * @file bench_jansson.cc
 * @brief Jansson benchmarks - separate TU to avoid json_object conflict with json-c.
 */
#include <benchmark/benchmark.h>
#include <jansson.h>
#include <cstring>

extern "C" {
extern const char BENCH_JSON_SCALAR[];
extern const char BENCH_JSON_NESTED[];
extern const char BENCH_JSON_STRING_HEAVY[];
}

static void BM_jansson_marshal_scalar(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        json_t *r = json_object();
        json_object_set_new(r, "int_val1", json_integer(1));
        json_object_set_new(r, "int_val2", json_integer(1));
        json_object_set_new(r, "long_val", json_integer(2));
        json_object_set_new(r, "double_val", json_real(3.0));
        json_object_set_new(r, "float_val", json_real(4.0));
        json_object_set_new(r, "sstr_val", json_string("hello this is string"));
        char *o = json_dumps(r, JSON_COMPACT);
        t += strlen(o);
        free(o);
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jansson_unmarshal_scalar(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_SCALAR), t = 0;
    for (auto _ : s) {
        json_error_t err;
        json_t *r = json_loads(BENCH_JSON_SCALAR, 0, &err);
        benchmark::DoNotOptimize(json_integer_value(json_object_get(r, "int_val1")));
        benchmark::DoNotOptimize(json_integer_value(json_object_get(r, "int_val2")));
        benchmark::DoNotOptimize(json_integer_value(json_object_get(r, "long_val")));
        benchmark::DoNotOptimize(json_real_value(json_object_get(r, "double_val")));
        benchmark::DoNotOptimize(json_real_value(json_object_get(r, "float_val")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "sstr_val")));
        t += l;
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jansson_marshal_nested(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        json_t *r = json_object();
        json_object_set_new(r, "name", json_string("John Doe"));
        json_object_set_new(r, "age", json_integer(42));
        json_t *h = json_object();
        json_object_set_new(h, "street", json_string("123 Main St"));
        json_object_set_new(h, "city", json_string("Springfield"));
        json_object_set_new(h, "state", json_string("IL"));
        json_object_set_new(h, "zip", json_string("62701"));
        json_object_set_new(r, "home_addr", h);
        json_t *w = json_object();
        json_object_set_new(w, "street", json_string("456 Oak Ave"));
        json_object_set_new(w, "city", json_string("Chicago"));
        json_object_set_new(w, "state", json_string("IL"));
        json_object_set_new(w, "zip", json_string("60601"));
        json_object_set_new(r, "work_addr", w);
        char *o = json_dumps(r, JSON_COMPACT);
        t += strlen(o);
        free(o);
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jansson_unmarshal_nested(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_NESTED), t = 0;
    for (auto _ : s) {
        json_error_t err;
        json_t *r = json_loads(BENCH_JSON_NESTED, 0, &err);
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "name")));
        benchmark::DoNotOptimize(json_integer_value(json_object_get(r, "age")));
        json_t *h = json_object_get(r, "home_addr");
        benchmark::DoNotOptimize(json_string_value(json_object_get(h, "street")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(h, "city")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(h, "state")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(h, "zip")));
        json_t *w = json_object_get(r, "work_addr");
        benchmark::DoNotOptimize(json_string_value(json_object_get(w, "street")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(w, "city")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(w, "state")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(w, "zip")));
        t += l;
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jansson_marshal_string_heavy(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        json_t *r = json_object();
        json_object_set_new(r, "first_name", json_string("Alexander"));
        json_object_set_new(r, "last_name", json_string("Constantinovich"));
        json_object_set_new(r, "email", json_string("alexander.constantinovich@example.com"));
        json_object_set_new(r, "phone", json_string("+1-555-123-4567"));
        json_object_set_new(r, "bio", json_string("A software engineer with over 15 years of experience in distributed systems and compiler design."));
        json_object_set_new(r, "website", json_string("https://example.com/alexander"));
        json_object_set_new(r, "company", json_string("Acme Corporation International"));
        json_object_set_new(r, "title", json_string("Principal Software Engineer"));
        char *o = json_dumps(r, JSON_COMPACT);
        t += strlen(o);
        free(o);
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jansson_unmarshal_string_heavy(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_STRING_HEAVY), t = 0;
    for (auto _ : s) {
        json_error_t err;
        json_t *r = json_loads(BENCH_JSON_STRING_HEAVY, 0, &err);
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "first_name")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "last_name")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "email")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "phone")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "bio")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "website")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "company")));
        benchmark::DoNotOptimize(json_string_value(json_object_get(r, "title")));
        t += l;
        json_decref(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

void register_jansson_benchmarks() {
    benchmark::RegisterBenchmark("BM_jansson_marshal_scalar", BM_jansson_marshal_scalar);
    benchmark::RegisterBenchmark("BM_jansson_unmarshal_scalar", BM_jansson_unmarshal_scalar);
    benchmark::RegisterBenchmark("BM_jansson_marshal_nested", BM_jansson_marshal_nested);
    benchmark::RegisterBenchmark("BM_jansson_unmarshal_nested", BM_jansson_unmarshal_nested);
    benchmark::RegisterBenchmark("BM_jansson_marshal_string_heavy", BM_jansson_marshal_string_heavy);
    benchmark::RegisterBenchmark("BM_jansson_unmarshal_string_heavy", BM_jansson_unmarshal_string_heavy);
}
