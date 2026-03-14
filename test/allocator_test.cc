#include <gtest/gtest.h>
#include <atomic>

extern "C" {
#include "json.gen.h"
}

/* ── Counting allocator ───────────────────────────────────────────── */

static std::atomic<int> g_malloc_count{0};
static std::atomic<int> g_realloc_count{0};
static std::atomic<int> g_free_count{0};

static void* counting_malloc(size_t sz) {
    g_malloc_count++;
    return malloc(sz);
}
static void* counting_realloc(void* p, size_t sz) {
    g_realloc_count++;
    return realloc(p, sz);
}
static void counting_free(void* p) {
    g_free_count++;
    free(p);
}

static void reset_counters() {
    g_malloc_count = 0;
    g_realloc_count = 0;
    g_free_count = 0;
}

/* ── Tests ────────────────────────────────────────────────────────── */

class AllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        reset_counters();
        json_gen_c_set_alloc(counting_malloc, counting_realloc, counting_free);
    }
    void TearDown() override {
        // Restore defaults
        json_gen_c_set_alloc(nullptr, nullptr, nullptr);
    }
};

TEST_F(AllocatorTest, UnmarshalArrayUsesCustomMalloc) {
    sstr_t json = sstr("{\"simple_int\":1,"
                        "\"int_array\":[10,20,30],"
                        "\"simple_string\":\"test\"}");
    struct ComplexStruct cs;
    ComplexStruct_init(&cs);
    json_unmarshal_ComplexStruct(json, &cs);

    EXPECT_GT(g_realloc_count.load(), 0)
        << "Expected custom realloc during unmarshal (array growth)";
    EXPECT_EQ(cs.int_array_len, 3);

    ComplexStruct_clear(&cs);
    sstr_free(json);
}

TEST_F(AllocatorTest, UnmarshalNestedStructUsesCustomMalloc) {
    sstr_t json = sstr("{\"simple_int\":1,"
                        "\"contacts\":[{\"name\":\"Alice\",\"age\":\"30\"}]}");
    struct ComplexStruct cs;
    ComplexStruct_init(&cs);
    json_unmarshal_ComplexStruct(json, &cs);

    EXPECT_GT(g_malloc_count.load(), 0)
        << "Expected custom malloc during struct array unmarshal";
    EXPECT_EQ(cs.contacts_len, 1);

    ComplexStruct_clear(&cs);
    sstr_free(json);
}

TEST_F(AllocatorTest, FreeUsesCustomAllocator) {
    sstr_t json = sstr("{\"simple_int\":1,"
                        "\"int_array\":[1,2,3]}");
    struct ComplexStruct cs;
    ComplexStruct_init(&cs);
    json_unmarshal_ComplexStruct(json, &cs);

    reset_counters();
    ComplexStruct_clear(&cs);

    EXPECT_GT(g_free_count.load(), 0)
        << "Expected custom free during clear";

    sstr_free(json);
}

TEST_F(AllocatorTest, NullResetsToDefault) {
    json_gen_c_set_alloc(nullptr, nullptr, nullptr);
    reset_counters();

    sstr_t json = sstr("{\"simple_int\":1,"
                        "\"int_array\":[1,2,3]}");
    struct ComplexStruct cs;
    ComplexStruct_init(&cs);
    json_unmarshal_ComplexStruct(json, &cs);

    // With NULL hooks, counters should stay at zero (defaults used)
    EXPECT_EQ(g_malloc_count.load(), 0);

    ComplexStruct_clear(&cs);
    sstr_free(json);
}
