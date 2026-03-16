#include <benchmark/benchmark.h>
#include <cjson/cJSON.h>
#include <cstring>

#include "json.gen.h"

// ============================================================================
// json-gen-c scalar benchmarks
// ============================================================================

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

// ============================================================================
// json-gen-c nested struct benchmarks
// ============================================================================

static void init_address(struct address* a, const char* street, const char* city,
                         const char* st, const char* zip) {
    address_init(a);
    a->street = sstr(street);
    a->city = sstr(city);
    a->state = sstr(st);
    a->zip = sstr(zip);
}

static void BM_marshal_nested(benchmark::State& state) {
    struct nested obj;
    nested_init(&obj);
    obj.name = sstr("John Doe");
    obj.age = 42;
    init_address(&obj.home_addr, "123 Main St", "Springfield", "IL", "62701");
    init_address(&obj.work_addr, "456 Oak Ave", "Chicago", "IL", "60601");
    sstr_t out = sstr_new();
    size_t total = 0;
    for (auto _ : state) {
        json_marshal_nested(&obj, out);
        total += sstr_length(out);
        sstr_clear(out);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(out);
    nested_clear(&obj);
}

static void BM_unmarshal_nested(benchmark::State& state) {
    struct nested obj;
    nested_init(&obj);
    obj.name = sstr("John Doe");
    obj.age = 42;
    init_address(&obj.home_addr, "123 Main St", "Springfield", "IL", "62701");
    init_address(&obj.work_addr, "456 Oak Ave", "Chicago", "IL", "60601");
    sstr_t content = sstr_new();
    json_marshal_nested(&obj, content);
    size_t total = 0;
    for (auto _ : state) {
        nested_clear(&obj);
        json_unmarshal_nested(content, &obj);
        total += sstr_length(content);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
    nested_clear(&obj);
}

static void BM_unmarshal_nested_selected(benchmark::State& state) {
    struct nested seed;
    nested_init(&seed);
    seed.name = sstr("John Doe");
    seed.age = 42;
    init_address(&seed.home_addr, "123 Main St", "Springfield", "IL", "62701");
    init_address(&seed.work_addr, "456 Oak Ave", "Chicago", "IL", "60601");

    sstr_t content = sstr_new();
    json_marshal_nested(&seed, content);

    uint64_t field_mask[nested_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(field_mask, nested_FIELD_name);
    JSON_GEN_C_FIELD_MASK_SET(field_mask, nested_FIELD_age);

    size_t total = 0;
    for (auto _ : state) {
        struct nested obj;
        nested_init(&obj);
        json_unmarshal_selected_nested(
            content, &obj, field_mask, nested_FIELD_MASK_WORD_COUNT);
        total += sstr_length(content);
        nested_clear(&obj);
    }

    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
    nested_clear(&seed);
}

// ============================================================================
// json-gen-c string-heavy struct benchmarks
// ============================================================================

static void BM_marshal_string_heavy(benchmark::State& state) {
    struct string_heavy obj;
    string_heavy_init(&obj);
    obj.first_name = sstr("Alexander");
    obj.last_name = sstr("Constantinovich");
    obj.email = sstr("alexander.constantinovich@example.com");
    obj.phone = sstr("+1-555-123-4567");
    obj.bio = sstr("A software engineer with over 15 years of experience in distributed systems and compiler design.");
    obj.website = sstr("https://example.com/alexander");
    obj.company = sstr("Acme Corporation International");
    obj.title = sstr("Principal Software Engineer");
    sstr_t out = sstr_new();
    size_t total = 0;
    for (auto _ : state) {
        json_marshal_string_heavy(&obj, out);
        total += sstr_length(out);
        sstr_clear(out);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(out);
    string_heavy_clear(&obj);
}

static void BM_unmarshal_string_heavy(benchmark::State& state) {
    struct string_heavy obj;
    string_heavy_init(&obj);
    obj.first_name = sstr("Alexander");
    obj.last_name = sstr("Constantinovich");
    obj.email = sstr("alexander.constantinovich@example.com");
    obj.phone = sstr("+1-555-123-4567");
    obj.bio = sstr("A software engineer with over 15 years of experience in distributed systems and compiler design.");
    obj.website = sstr("https://example.com/alexander");
    obj.company = sstr("Acme Corporation International");
    obj.title = sstr("Principal Software Engineer");
    sstr_t content = sstr_new();
    json_marshal_string_heavy(&obj, content);
    size_t total = 0;
    for (auto _ : state) {
        string_heavy_clear(&obj);
        json_unmarshal_string_heavy(content, &obj);
        total += sstr_length(content);
    }
    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
    string_heavy_clear(&obj);
}

static void BM_unmarshal_string_heavy_selected(benchmark::State& state) {
    struct string_heavy seed;
    string_heavy_init(&seed);
    seed.first_name = sstr("Alexander");
    seed.last_name = sstr("Constantinovich");
    seed.email = sstr("alexander.constantinovich@example.com");
    seed.phone = sstr("+1-555-123-4567");
    seed.bio = sstr("A software engineer with over 15 years of experience in distributed systems and compiler design.");
    seed.website = sstr("https://example.com/alexander");
    seed.company = sstr("Acme Corporation International");
    seed.title = sstr("Principal Software Engineer");

    sstr_t content = sstr_new();
    json_marshal_string_heavy(&seed, content);

    uint64_t field_mask[string_heavy_FIELD_MASK_WORD_COUNT] = {0};
    JSON_GEN_C_FIELD_MASK_SET(field_mask, string_heavy_FIELD_email);
    JSON_GEN_C_FIELD_MASK_SET(field_mask, string_heavy_FIELD_phone);

    size_t total = 0;
    for (auto _ : state) {
        struct string_heavy obj;
        string_heavy_init(&obj);
        json_unmarshal_selected_string_heavy(
            content, &obj, field_mask, string_heavy_FIELD_MASK_WORD_COUNT);
        total += sstr_length(content);
        string_heavy_clear(&obj);
    }

    state.SetBytesProcessed((int64_t)total);
    sstr_free(content);
    string_heavy_clear(&seed);
}

// ============================================================================
// cJSON comparison benchmarks (scalar)
// ============================================================================

static const char* cjson_scalar_json =
    "{\"int_val1\":1,\"int_val2\":1,\"long_val\":2,"
    "\"double_val\":3.0,\"float_val\":4.0,"
    "\"sstr_val\":\"hello this is string\"}";

static void BM_cjson_marshal_scalar(benchmark::State& state) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "int_val1", 1);
    cJSON_AddNumberToObject(root, "int_val2", 1);
    cJSON_AddNumberToObject(root, "long_val", 2);
    cJSON_AddNumberToObject(root, "double_val", 3.0);
    cJSON_AddNumberToObject(root, "float_val", 4.0);
    cJSON_AddStringToObject(root, "sstr_val", "hello this is string");
    size_t total = 0;
    for (auto _ : state) {
        char* out = cJSON_PrintUnformatted(root);
        total += strlen(out);
        cJSON_free(out);
    }
    state.SetBytesProcessed((int64_t)total);
    cJSON_Delete(root);
}

static void BM_cjson_unmarshal_scalar(benchmark::State& state) {
    size_t total = 0;
    size_t len = strlen(cjson_scalar_json);
    for (auto _ : state) {
        cJSON* root = cJSON_Parse(cjson_scalar_json);
        total += len;
        cJSON_Delete(root);
    }
    state.SetBytesProcessed((int64_t)total);
}

// ============================================================================
// cJSON comparison benchmarks (nested)
// ============================================================================

static const char* cjson_nested_json =
    "{\"name\":\"John Doe\",\"age\":42,"
    "\"home_addr\":{\"street\":\"123 Main St\",\"city\":\"Springfield\","
    "\"state\":\"IL\",\"zip\":\"62701\"},"
    "\"work_addr\":{\"street\":\"456 Oak Ave\",\"city\":\"Chicago\","
    "\"state\":\"IL\",\"zip\":\"60601\"}}";

static void BM_cjson_marshal_nested(benchmark::State& state) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "John Doe");
    cJSON_AddNumberToObject(root, "age", 42);
    cJSON* home = cJSON_AddObjectToObject(root, "home_addr");
    cJSON_AddStringToObject(home, "street", "123 Main St");
    cJSON_AddStringToObject(home, "city", "Springfield");
    cJSON_AddStringToObject(home, "state", "IL");
    cJSON_AddStringToObject(home, "zip", "62701");
    cJSON* work = cJSON_AddObjectToObject(root, "work_addr");
    cJSON_AddStringToObject(work, "street", "456 Oak Ave");
    cJSON_AddStringToObject(work, "city", "Chicago");
    cJSON_AddStringToObject(work, "state", "IL");
    cJSON_AddStringToObject(work, "zip", "60601");
    size_t total = 0;
    for (auto _ : state) {
        char* out = cJSON_PrintUnformatted(root);
        total += strlen(out);
        cJSON_free(out);
    }
    state.SetBytesProcessed((int64_t)total);
    cJSON_Delete(root);
}

static void BM_cjson_unmarshal_nested(benchmark::State& state) {
    size_t total = 0;
    size_t len = strlen(cjson_nested_json);
    for (auto _ : state) {
        cJSON* root = cJSON_Parse(cjson_nested_json);
        total += len;
        cJSON_Delete(root);
    }
    state.SetBytesProcessed((int64_t)total);
}

// ============================================================================
// Register benchmarks
// ============================================================================

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
BENCHMARK(BM_marshal_nested);
BENCHMARK(BM_unmarshal_nested);
BENCHMARK(BM_unmarshal_nested_selected);
BENCHMARK(BM_marshal_string_heavy);
BENCHMARK(BM_unmarshal_string_heavy);
BENCHMARK(BM_unmarshal_string_heavy_selected);
BENCHMARK(BM_cjson_marshal_scalar);
BENCHMARK(BM_cjson_unmarshal_scalar);
BENCHMARK(BM_cjson_marshal_nested);
BENCHMARK(BM_cjson_unmarshal_nested);

BENCHMARK_MAIN();
