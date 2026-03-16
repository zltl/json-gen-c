/**
 * @file diagnostic_test.cc
 * @brief Test suite for the diagnostic engine and parser diagnostic integration.
 */

#define _POSIX_C_SOURCE 200809L

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <unistd.h>

extern "C" {
#include "struct/struct_parse.h"
#include "utils/diag.h"
#include "utils/hash_map.h"
#include "utils/sstr.h"
}

class DiagEngineTest : public ::testing::Test {
protected:
    struct diag_engine* engine = nullptr;
    const char* filename = "test.json-gen-c";
    const char* source = "line one\nline two\nline three\n";
    long source_len = 0;

    void SetUp() override {
        source_len = (long)strlen(source);
        engine = diag_engine_new(filename, source, source_len);
        ASSERT_NE(nullptr, engine);
    }

    void TearDown() override {
        if (engine) {
            diag_engine_free(engine);
            engine = nullptr;
        }
    }
};

TEST_F(DiagEngineTest, NewEngineIsEmpty) {
    EXPECT_EQ(0, engine->count);
    EXPECT_EQ(0, engine->error_count);
    EXPECT_EQ(0, engine->warning_count);
    EXPECT_FALSE(diag_has_errors(engine));
    EXPECT_EQ(0, diag_error_count(engine));
}

TEST_F(DiagEngineTest, NewEngineStoresMetadata) {
    EXPECT_STREQ(filename, engine->filename);
    EXPECT_EQ(source, engine->source);
    EXPECT_EQ(source_len, engine->source_len);
}

TEST_F(DiagEngineTest, EmitError) {
    diag_emit(engine, DIAG_ERROR, 1, 5, "unexpected token '%s'", "blah");

    EXPECT_EQ(1, engine->count);
    EXPECT_EQ(1, engine->error_count);
    EXPECT_EQ(0, engine->warning_count);
    EXPECT_TRUE(diag_has_errors(engine));
    EXPECT_EQ(1, diag_error_count(engine));

    EXPECT_EQ(DIAG_ERROR, engine->entries[0].severity);
    EXPECT_EQ(1, engine->entries[0].line);
    EXPECT_EQ(5, engine->entries[0].col);
    EXPECT_STREQ("unexpected token 'blah'", engine->entries[0].message);
}

TEST_F(DiagEngineTest, EmitWarning) {
    diag_emit(engine, DIAG_WARNING, 2, 1, "unused field");

    EXPECT_EQ(1, engine->count);
    EXPECT_EQ(0, engine->error_count);
    EXPECT_EQ(1, engine->warning_count);
    EXPECT_FALSE(diag_has_errors(engine));
    EXPECT_EQ(DIAG_WARNING, engine->entries[0].severity);
    EXPECT_STREQ("unused field", engine->entries[0].message);
}

TEST_F(DiagEngineTest, EmitNote) {
    diag_emit(engine, DIAG_NOTE, 3, 1, "defined here");

    EXPECT_EQ(1, engine->count);
    EXPECT_EQ(0, engine->error_count);
    EXPECT_EQ(0, engine->warning_count);
    EXPECT_FALSE(diag_has_errors(engine));
    EXPECT_EQ(DIAG_NOTE, engine->entries[0].severity);
    EXPECT_STREQ("defined here", engine->entries[0].message);
}

TEST_F(DiagEngineTest, MultipleEmits) {
    diag_emit(engine, DIAG_ERROR, 1, 1, "error one");
    diag_emit(engine, DIAG_WARNING, 2, 1, "warning one");
    diag_emit(engine, DIAG_ERROR, 3, 1, "error two");
    diag_emit(engine, DIAG_NOTE, 1, 1, "note one");

    EXPECT_EQ(4, engine->count);
    EXPECT_EQ(2, engine->error_count);
    EXPECT_EQ(1, engine->warning_count);
    EXPECT_TRUE(diag_has_errors(engine));
    EXPECT_EQ(2, diag_error_count(engine));
}

TEST_F(DiagEngineTest, MaxErrorsLimit) {
    engine->max_errors = 3;

    for (int i = 0; i < 10; i++) {
        diag_emit(engine, DIAG_ERROR, 1, 1, "error %d", i);
    }

    EXPECT_EQ(3, engine->error_count);
    EXPECT_EQ(3, engine->count);
}

TEST_F(DiagEngineTest, MaxErrorsDoesNotLimitWarnings) {
    engine->max_errors = 2;

    diag_emit(engine, DIAG_ERROR, 1, 1, "error 1");
    diag_emit(engine, DIAG_ERROR, 2, 1, "error 2");
    diag_emit(engine, DIAG_ERROR, 3, 1, "error 3 should be dropped");
    diag_emit(engine, DIAG_WARNING, 1, 1, "warning should still work");

    EXPECT_EQ(2, engine->error_count);
    EXPECT_EQ(1, engine->warning_count);
    EXPECT_EQ(3, engine->count);
}

TEST_F(DiagEngineTest, FormatWithArguments) {
    diag_emit(engine, DIAG_ERROR, 1, 5, "expected '%s', found '%s'", ";", "}");
    EXPECT_STREQ("expected ';', found '}'", engine->entries[0].message);
}

TEST_F(DiagEngineTest, FormatWithIntegers) {
    diag_emit(engine, DIAG_ERROR, 1, 1, "array size must be > 0, got %d", -5);
    EXPECT_STREQ("array size must be > 0, got -5", engine->entries[0].message);
}

TEST_F(DiagEngineTest, GrowsBeyondInitialCapacity) {
    for (int i = 0; i < 20; i++) {
        diag_emit(engine, DIAG_WARNING, 1, 1, "warning %d", i);
    }

    EXPECT_EQ(20, engine->count);
    EXPECT_EQ(20, engine->warning_count);

    for (int i = 0; i < 20; i++) {
        std::string expected = "warning " + std::to_string(i);
        EXPECT_STREQ(expected.c_str(), engine->entries[i].message);
    }
}

class DiagOutputTest : public ::testing::Test {
protected:
    std::string capture_output(struct diag_engine* engine) {
        char tmpname[] = "/tmp/diag_test_XXXXXX";
        int fd = mkstemp(tmpname);
        EXPECT_NE(-1, fd);
        FILE* file = fdopen(fd, "w+");
        EXPECT_NE(nullptr, file);

        diag_print_all(engine, file);

        fflush(file);
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::string result;
        if (size > 0) {
            result.resize((size_t)size);
            size_t nread = fread(&result[0], 1, (size_t)size, file);
            result.resize(nread);
        }

        fclose(file);
        unlink(tmpname);
        return result;
    }
};

TEST_F(DiagOutputTest, ClangStyleFormat) {
    const char* src = "int x = bad;\n";
    struct diag_engine* engine = diag_engine_new("foo.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 9, "undeclared identifier 'bad'");

    std::string output = capture_output(engine);

    EXPECT_NE(std::string::npos,
              output.find("foo.c:1:9: error: undeclared identifier 'bad'"));
    EXPECT_NE(std::string::npos, output.find("int x = bad;"));
    EXPECT_NE(std::string::npos, output.find("        ^"));
    EXPECT_NE(std::string::npos, output.find("1 error generated."));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, MultiLineSource) {
    const char* src = "struct Foo {\n    int x\n    double y;\n}\n";
    struct diag_engine* engine =
        diag_engine_new("test.schema", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 2, 10, "expected ';'");

    std::string output = capture_output(engine);

    EXPECT_NE(std::string::npos,
              output.find("test.schema:2:10: error: expected ';'"));
    EXPECT_NE(std::string::npos, output.find("    int x"));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, WarningFormat) {
    const char* src = "test\n";
    struct diag_engine* engine = diag_engine_new("w.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_WARNING, 1, 1, "something fishy");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos,
              output.find("w.c:1:1: warning: something fishy"));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, NoteFormat) {
    const char* src = "test\n";
    struct diag_engine* engine = diag_engine_new("n.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_NOTE, 1, 1, "see declaration");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos, output.find("n.c:1:1: note: see declaration"));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, MultipleErrorsSummary) {
    const char* src = "a\nb\nc\n";
    struct diag_engine* engine = diag_engine_new("m.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 1, "e1");
    diag_emit(engine, DIAG_ERROR, 2, 1, "e2");
    diag_emit(engine, DIAG_ERROR, 3, 1, "e3");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos, output.find("3 errors generated."));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, ErrorsAndWarningsSummary) {
    const char* src = "a\nb\n";
    struct diag_engine* engine = diag_engine_new("ew.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 1, "e1");
    diag_emit(engine, DIAG_WARNING, 2, 1, "w1");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos,
              output.find("1 error and 1 warning generated."));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, PluralSummary) {
    const char* src = "a\nb\nc\nd\n";
    struct diag_engine* engine = diag_engine_new("p.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 1, "e1");
    diag_emit(engine, DIAG_ERROR, 2, 1, "e2");
    diag_emit(engine, DIAG_WARNING, 3, 1, "w1");
    diag_emit(engine, DIAG_WARNING, 4, 1, "w2");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos,
              output.find("2 errors and 2 warnings generated."));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, SingleErrorSingularSummary) {
    const char* src = "a\n";
    struct diag_engine* engine = diag_engine_new("s.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 1, "e1");

    std::string output = capture_output(engine);
    EXPECT_NE(std::string::npos, output.find("1 error generated."));
    EXPECT_EQ(std::string::npos, output.find("1 errors"));

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, EmptyEngineProducesNoOutput) {
    const char* src = "a\n";
    struct diag_engine* engine = diag_engine_new("e.c", src, (long)strlen(src));

    std::string output = capture_output(engine);
    EXPECT_TRUE(output.empty());

    diag_engine_free(engine);
}

TEST_F(DiagOutputTest, NoAnsiCodesWhenNotTty) {
    const char* src = "bad line\n";
    struct diag_engine* engine =
        diag_engine_new("no_ansi.c", src, (long)strlen(src));
    diag_emit(engine, DIAG_ERROR, 1, 1, "test");

    std::string output = capture_output(engine);
    EXPECT_EQ(std::string::npos, output.find("\033["));

    diag_engine_free(engine);
}

TEST(DiagNullSafety, FreeNull) {
    diag_engine_free(nullptr);
}

TEST(DiagNullSafety, EmitNull) {
    diag_emit(nullptr, DIAG_ERROR, 1, 1, "test");
}

TEST(DiagNullSafety, PrintNull) {
    diag_print_all(nullptr, stderr);
}

TEST(DiagNullSafety, HasErrorsNull) {
    EXPECT_EQ(0, diag_has_errors(nullptr));
}

TEST(DiagNullSafety, ErrorCountNull) {
    EXPECT_EQ(0, diag_error_count(nullptr));
}

class ParserDiagTest : public ::testing::Test {
protected:
    struct struct_parser* parser = nullptr;

    void SetUp() override {
        parser = struct_parser_new();
        ASSERT_NE(nullptr, parser);
        parser->name = const_cast<char*>("test_input.json-gen-c");
    }

    void TearDown() override {
        if (parser) {
            struct_parser_free(parser);
            parser = nullptr;
        }
    }

    int parse(const char* content) {
        sstr_t s = sstr(content);
        int r = struct_parser_parse(parser, s);
        sstr_free(s);
        return r;
    }

    bool diag_contains(const char* needle) const {
        if (parser == nullptr || parser->diag == nullptr) {
            return false;
        }
        for (int i = 0; i < parser->diag->count; i++) {
            if (strstr(parser->diag->entries[i].message, needle) != nullptr) {
                return true;
            }
        }
        return false;
    }

    bool has_struct(const char* name) const {
        sstr_t key = sstr(name);
        void* value = nullptr;
        int found = hash_map_find(parser->struct_map, key, &value);
        sstr_free(key);
        return found == HASH_MAP_OK && value != nullptr;
    }

    bool has_enum(const char* name) const {
        sstr_t key = sstr(name);
        void* value = nullptr;
        int found = hash_map_find(parser->enum_map, key, &value);
        sstr_free(key);
        return found == HASH_MAP_OK && value != nullptr;
    }
};

TEST_F(ParserDiagTest, ValidSchemaNoErrors) {
    int r = parse("struct Foo { int x; double y; }");
    EXPECT_EQ(0, r);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_FALSE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, ValidEnumNoErrors) {
    int r = parse("enum Color { RED, GREEN, BLUE }");
    EXPECT_EQ(0, r);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_FALSE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, MissingSemicolonInField) {
    int r = parse("struct Foo { int x\n double y; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
    EXPECT_GE(diag_error_count(parser->diag), 1);

    bool found_semicolon_error = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "';'") != nullptr) {
            found_semicolon_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_semicolon_error);
}

TEST_F(ParserDiagTest, MissingSemicolonRecovery) {
    int r = parse("struct Foo {\n    int a\n    double b\n    sstr_t c;\n}");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_GE(diag_error_count(parser->diag), 1);
}

TEST_F(ParserDiagTest, UnexpectedTopLevelToken) {
    int r = parse("garbage_token");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "struct") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ParserDiagTest, MultipleTopLevelErrors) {
    int r = parse("bad1\nstruct Foo { int x; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_GE(diag_error_count(parser->diag), 1);
}

TEST_F(ParserDiagTest, TrailingTopLevelTokenRecoveryKeepsFollowingEnum) {
    int r = parse("struct Good { int x; }\n"
                  "trailing_token\n"
                  "enum State { READY, DONE }\n");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
    EXPECT_TRUE(has_struct("Good"));
    EXPECT_TRUE(has_enum("State"));
}

TEST_F(ParserDiagTest, EnumMissingName) {
    int r = parse("enum { X, Y }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "enum name") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ParserDiagTest, EnumMissingBrace) {
    int r = parse("enum Broken { A, B");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, EnumBadSeparator) {
    int r = parse("enum Color {\n    RED\n    GREEN\n}");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, EnumRecoveryDoesNotCrash) {
    int r = parse("enum Color {\n    RED,\n    ,\n    GREEN\n}\nstruct Foo { int x; }");
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(r == 0 || r < 0);
}

TEST_F(ParserDiagTest, StructMissingOpenBrace) {
    int r = parse("struct Foo int x; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "'{'") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ParserDiagTest, ReservedKeywordStruct) {
    int r = parse("struct Foo { struct bar; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, MapMissingParams) {
    int r = parse("struct Foo { map bar; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, ArrayMissingCloseBracket) {
    int r = parse("struct Foo { int arr[5; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "']'") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ParserDiagTest, RecoveryParsesValidStructAfterError) {
    int r = parse("struct Bad { int a\n double b; }\nstruct Good { sstr_t name; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    sstr_t key = sstr("Good");
    void* value = nullptr;
    int found = hash_map_find(parser->struct_map, key, &value);
    EXPECT_EQ(HASH_MAP_OK, found);
    EXPECT_NE(nullptr, value);
    sstr_free(key);
}

TEST_F(ParserDiagTest, ErrorPositionIsCorrect) {
    int r = parse("struct Foo {\n    int x\n    double y;\n}");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    ASSERT_GE(parser->diag->count, 1);

    struct diag_entry* entry = &parser->diag->entries[0];
    EXPECT_EQ(DIAG_ERROR, entry->severity);
    EXPECT_EQ(3, entry->line);
}

TEST_F(ParserDiagTest, IncludeBadFileProducesError) {
    int r = parse("#include \"nonexistent_file.json-gen-c\"");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "not found") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ParserDiagTest, IncludeNotFollowedByString) {
    int r = parse("#include bad_token");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, IncludeErrorDoesNotBlockFollowingStruct) {
    int r = parse("#include \"nonexistent_file.json-gen-c\"\n"
                  "struct Good { int x; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
    EXPECT_TRUE(diag_contains("not found"));
    EXPECT_TRUE(has_struct("Good"));
}

TEST_F(ParserDiagTest, EmptyInputNoErrors) {
    int r = parse("");
    EXPECT_EQ(0, r);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_FALSE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, WhitespaceOnlyInputNoErrors) {
    int r = parse("  \n\t  \r\n");
    EXPECT_EQ(0, r);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_FALSE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, OnlyCommentsNoErrors) {
    int r = parse("// just a comment\n/* also a comment */\n");
    EXPECT_EQ(0, r);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_FALSE(diag_has_errors(parser->diag));
}

TEST_F(ParserDiagTest, CommentThenUnexpectedTopLevelToken) {
    int r = parse("// just a comment\n/* also a comment */\ntrailing_token");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));
    EXPECT_TRUE(
        diag_contains("expected 'struct', 'enum', 'oneof' or '#include'"));
}

TEST_F(ParserDiagTest, MultipleStructAndEnumErrors) {
    const char* schema =
        "enum Broken { A\n B }\n"
        "struct Bad {\n"
        "    int a\n"
        "    sstr_t b;\n"
        "}\n"
        "struct AlsoBad {\n"
        "    double x\n"
        "    int y;\n"
        "}\n";

    int r = parse(schema);
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_GE(diag_error_count(parser->diag), 3);
}

// ============================================================
// Schema Validation Tests
// ============================================================

class SchemaValidationTest : public ::testing::Test {
protected:
    struct struct_parser* parser = nullptr;

    void SetUp() override {
        parser = struct_parser_new();
        ASSERT_NE(nullptr, parser);
        parser->name = const_cast<char*>("test_input.json-gen-c");
    }

    void TearDown() override {
        if (parser) {
            struct_parser_free(parser);
            parser = nullptr;
        }
    }

    // Returns parse result code
    int parse(const char* content) {
        sstr_t s = sstr(content);
        int r = struct_parser_parse(parser, s);
        sstr_free(s);
        return r;
    }

    // Parse then validate; returns validate result code (0=ok, -1=error)
    int parse_and_validate(const char* content) {
        int r = parse(content);
        if (r < 0) return r;
        return struct_parser_validate(parser);
    }
};

TEST_F(SchemaValidationTest, ValidSchemaNoValidationErrors) {
    int r = parse_and_validate(
        "enum Color { RED, GREEN, BLUE }\n"
        "struct Foo { int x; Color c; sstr_t name; }");
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, UndefinedStructRef) {
    int r = parse_and_validate(
        "struct Foo { UnknownStruct bar; }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, UndefinedMapValueStructRef) {
    int r = parse_and_validate(
        "struct Foo { map<sstr_t, UnknownStruct> m; }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, SelfReferencingStructNoError) {
    // Self-reference exists in struct_map, so validation passes.
    // Circular dependency is caught later by gencode.
    int r = parse_and_validate(
        "struct Node { int val; Node child; }");
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, DuplicateStructName) {
    // Inline dup check during parsing should catch this
    int r = parse("struct Dup { int x; }\nstruct Dup { int y; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "duplicate struct name") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SchemaValidationTest, DuplicateEnumName) {
    int r = parse("enum Dup { X, Y }\nenum Dup { A, B }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "duplicate enum name") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SchemaValidationTest, StructEnumNameClash) {
    int r = parse("enum Clash { X, Y }\nstruct Clash { int x; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message, "already defined as an enum") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SchemaValidationTest, OneofStructNameClash) {
    int r = parse("struct Shape { int kind; }\n"
                  "oneof Shape { Shape circle; }");
    EXPECT_LT(r, 0);
    ASSERT_NE(nullptr, parser->diag);
    EXPECT_TRUE(diag_has_errors(parser->diag));

    bool found = false;
    for (int i = 0; i < parser->diag->count; i++) {
        if (strstr(parser->diag->entries[i].message,
                   "already defined as a struct") != nullptr) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SchemaValidationTest, DuplicateFieldName) {
    int r = parse_and_validate(
        "struct Bad { int x; double x; }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, DuplicateEnumValue) {
    int r = parse_and_validate(
        "enum Bad { A, B, A }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, CKeywordFieldWarning) {
    // C keywords as field names should produce warnings, not errors.
    // parse succeeds, validate returns 0 (warnings don't fail).
    int r = parse_and_validate(
        "struct Foo { int return; double volatile; }");
    // Note: 'return' and 'volatile' are valid identifiers to the tokenizer
    // but C keywords in the validation.  Validate should still return 0
    // because warnings don't count as errors.
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, CKeywordStructNameWarning) {
    int r = parse_and_validate("struct void { int x; }");
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, CKeywordEnumNameWarning) {
    int r = parse_and_validate("enum while { X, Y }");
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, MultipleValidationErrors) {
    // Both undefined ref and duplicate field in same schema
    int r = parse_and_validate(
        "struct Bad { int x; int x; UnknownType y; }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, EnumRefValid) {
    // Regression guard: field referencing a defined enum should not error
    int r = parse_and_validate(
        "enum Status { OK, ERR }\n"
        "struct Foo { Status s; }");
    EXPECT_EQ(0, r);
}

TEST_F(SchemaValidationTest, UndefinedEnumRefInSchema) {
    // Field references type not in either struct_map or enum_map
    int r = parse_and_validate(
        "struct BadRef { UnknownEnum val; }");
    EXPECT_LT(r, 0);
}

TEST_F(SchemaValidationTest, MultipleStructsAllValid) {
    // Multiple structs referencing each other
    int r = parse_and_validate(
        "struct Inner { int x; }\n"
        "struct Outer { Inner item; int count; }");
    EXPECT_EQ(0, r);
}
