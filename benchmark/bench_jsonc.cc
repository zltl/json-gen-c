/**
 * @file bench_jsonc.cc
 * @brief json-c benchmarks - separate TU to avoid json_object conflict with jansson.
 */
#include <benchmark/benchmark.h>
#include <json-c/json.h>
#include <cstring>

extern "C" {
extern const char BENCH_JSON_SCALAR[];
extern const char BENCH_JSON_NESTED[];
extern const char BENCH_JSON_STRING_HEAVY[];
}

static void BM_jsonc_marshal_scalar(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        struct json_object *r = json_object_new_object();
        json_object_object_add(r, "int_val1", json_object_new_int(1));
        json_object_object_add(r, "int_val2", json_object_new_int(1));
        json_object_object_add(r, "long_val", json_object_new_int64(2));
        json_object_object_add(r, "double_val", json_object_new_double(3.0));
        json_object_object_add(r, "float_val", json_object_new_double(4.0));
        json_object_object_add(r, "sstr_val", json_object_new_string("hello this is string"));
        const char *o = json_object_to_json_string_ext(r, JSON_C_TO_STRING_PLAIN);
        t += strlen(o);
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jsonc_unmarshal_scalar(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_SCALAR), t = 0;
    for (auto _ : s) {
        struct json_object *r = json_tokener_parse(BENCH_JSON_SCALAR);
        struct json_object *v;
        json_object_object_get_ex(r, "int_val1", &v);
        benchmark::DoNotOptimize(json_object_get_int(v));
        json_object_object_get_ex(r, "int_val2", &v);
        benchmark::DoNotOptimize(json_object_get_int(v));
        json_object_object_get_ex(r, "long_val", &v);
        benchmark::DoNotOptimize(json_object_get_int64(v));
        json_object_object_get_ex(r, "double_val", &v);
        benchmark::DoNotOptimize(json_object_get_double(v));
        json_object_object_get_ex(r, "float_val", &v);
        benchmark::DoNotOptimize(json_object_get_double(v));
        json_object_object_get_ex(r, "sstr_val", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        t += l;
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jsonc_marshal_nested(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        struct json_object *r = json_object_new_object();
        json_object_object_add(r, "name", json_object_new_string("John Doe"));
        json_object_object_add(r, "age", json_object_new_int(42));
        struct json_object *h = json_object_new_object();
        json_object_object_add(h, "street", json_object_new_string("123 Main St"));
        json_object_object_add(h, "city", json_object_new_string("Springfield"));
        json_object_object_add(h, "state", json_object_new_string("IL"));
        json_object_object_add(h, "zip", json_object_new_string("62701"));
        json_object_object_add(r, "home_addr", h);
        struct json_object *w = json_object_new_object();
        json_object_object_add(w, "street", json_object_new_string("456 Oak Ave"));
        json_object_object_add(w, "city", json_object_new_string("Chicago"));
        json_object_object_add(w, "state", json_object_new_string("IL"));
        json_object_object_add(w, "zip", json_object_new_string("60601"));
        json_object_object_add(r, "work_addr", w);
        const char *o = json_object_to_json_string_ext(r, JSON_C_TO_STRING_PLAIN);
        t += strlen(o);
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jsonc_unmarshal_nested(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_NESTED), t = 0;
    for (auto _ : s) {
        struct json_object *r = json_tokener_parse(BENCH_JSON_NESTED);
        struct json_object *v, *sub;
        json_object_object_get_ex(r, "name", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "age", &v);
        benchmark::DoNotOptimize(json_object_get_int(v));
        json_object_object_get_ex(r, "home_addr", &sub);
        json_object_object_get_ex(sub, "street", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "city", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "state", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "zip", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "work_addr", &sub);
        json_object_object_get_ex(sub, "street", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "city", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "state", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(sub, "zip", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        t += l;
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jsonc_marshal_string_heavy(benchmark::State& s) {
    size_t t = 0;
    for (auto _ : s) {
        struct json_object *r = json_object_new_object();
        json_object_object_add(r, "first_name", json_object_new_string("Alexander"));
        json_object_object_add(r, "last_name", json_object_new_string("Constantinovich"));
        json_object_object_add(r, "email", json_object_new_string("alexander.constantinovich@example.com"));
        json_object_object_add(r, "phone", json_object_new_string("+1-555-123-4567"));
        json_object_object_add(r, "bio", json_object_new_string("A software engineer with over 15 years of experience in distributed systems and compiler design."));
        json_object_object_add(r, "website", json_object_new_string("https://example.com/alexander"));
        json_object_object_add(r, "company", json_object_new_string("Acme Corporation International"));
        json_object_object_add(r, "title", json_object_new_string("Principal Software Engineer"));
        const char *o = json_object_to_json_string_ext(r, JSON_C_TO_STRING_PLAIN);
        t += strlen(o);
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

static void BM_jsonc_unmarshal_string_heavy(benchmark::State& s) {
    size_t l = strlen(BENCH_JSON_STRING_HEAVY), t = 0;
    for (auto _ : s) {
        struct json_object *r = json_tokener_parse(BENCH_JSON_STRING_HEAVY);
        struct json_object *v;
        json_object_object_get_ex(r, "first_name", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "last_name", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "email", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "phone", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "bio", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "website", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "company", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        json_object_object_get_ex(r, "title", &v);
        benchmark::DoNotOptimize(json_object_get_string(v));
        t += l;
        json_object_put(r);
    }
    s.SetBytesProcessed((int64_t)t);
}

void register_jsonc_benchmarks() {
    benchmark::RegisterBenchmark("BM_jsonc_marshal_scalar", BM_jsonc_marshal_scalar);
    benchmark::RegisterBenchmark("BM_jsonc_unmarshal_scalar", BM_jsonc_unmarshal_scalar);
    benchmark::RegisterBenchmark("BM_jsonc_marshal_nested", BM_jsonc_marshal_nested);
    benchmark::RegisterBenchmark("BM_jsonc_unmarshal_nested", BM_jsonc_unmarshal_nested);
    benchmark::RegisterBenchmark("BM_jsonc_marshal_string_heavy", BM_jsonc_marshal_string_heavy);
    benchmark::RegisterBenchmark("BM_jsonc_unmarshal_string_heavy", BM_jsonc_unmarshal_string_heavy);
}
