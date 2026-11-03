#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/emitter/line_directive_tracker.hpp>

using namespace keety::emitter;
using namespace keety::diag;

TEST_CASE("LineDirectiveTracker initialization and enable toggle", "[emitter][line_mapping]") {
    LineDirectiveTracker tracker;
    REQUIRE(tracker.is_enabled());
    REQUIRE(tracker.get_last_line() == 0);
    REQUIRE(tracker.get_last_file().empty());

    tracker.set_enabled(false);
    REQUIRE_FALSE(tracker.is_enabled());
    REQUIRE(tracker.format_line(10, "main.keet").empty());

    tracker.set_enabled(true);
    REQUIRE(tracker.is_enabled());
    REQUIRE(tracker.format_line(10, "main.keet") == "#line 10 \"main.keet\"\n");
}

TEST_CASE("LineDirectiveTracker normalize_path", "[emitter][line_mapping]") {
    REQUIRE(LineDirectiveTracker::normalize_path("").empty());
    REQUIRE(LineDirectiveTracker::normalize_path("src/main.keet") == "src/main.keet");
    REQUIRE(LineDirectiveTracker::normalize_path("src\\nested\\module.keet") == "src/nested/module.keet");
    REQUIRE(LineDirectiveTracker::normalize_path("dir\\\"quoted\"\\file.keet") == "dir/\\\"quoted\\\"/file.keet");
}

TEST_CASE("LineDirectiveTracker deduplication and file changes", "[emitter][line_mapping]") {
    LineDirectiveTracker tracker;

    // First emission
    std::string d1 = tracker.format_line(10, "src/main.keet");
    REQUIRE(d1 == "#line 10 \"src/main.keet\"\n");
    REQUIRE(tracker.get_last_line() == 10);
    REQUIRE(tracker.get_last_file() == "src/main.keet");

    // Same line and file -> deduplicated, empty return
    std::string d2 = tracker.format_line(10, "src/main.keet");
    REQUIRE(d2.empty());

    // Advance line in same file
    std::string d3 = tracker.format_line(15, "src/main.keet");
    REQUIRE(d3 == "#line 15 \"src/main.keet\"\n");
    REQUIRE(tracker.get_last_line() == 15);

    // Switch to another file (same line number)
    std::string d4 = tracker.format_line(15, "src/math.keet");
    REQUIRE(d4 == "#line 15 \"src/math.keet\"\n");
    REQUIRE(tracker.get_last_file() == "src/math.keet");

    // Windows backslash path should be normalized and compared properly
    std::string d5 = tracker.format_line(15, "src\\math.keet");
    REQUIRE(d5.empty()); // Same line 15, same normalized path "src/math.keet"
}

TEST_CASE("LineDirectiveTracker line 0 and empty file", "[emitter][line_mapping]") {
    LineDirectiveTracker tracker;

    // Line 0 is invalid
    REQUIRE(tracker.format_line(0, "src/main.keet").empty());

    // Empty file path emits line number only
    std::string d = tracker.format_line(42, "");
    REQUIRE(d == "#line 42\n");
    REQUIRE(tracker.get_last_line() == 42);
    REQUIRE(tracker.get_last_file().empty());
}

TEST_CASE("LineDirectiveTracker reset", "[emitter][line_mapping]") {
    LineDirectiveTracker tracker;

    REQUIRE(tracker.format_line(5, "app.keet") == "#line 5 \"app.keet\"\n");
    REQUIRE(tracker.format_line(5, "app.keet").empty());

    tracker.reset();
    REQUIRE(tracker.get_last_line() == 0);
    REQUIRE(tracker.get_last_file().empty());

    // Re-emitting the same location after reset succeeds
    REQUIRE(tracker.format_line(5, "app.keet") == "#line 5 \"app.keet\"\n");
}

TEST_CASE("LineDirectiveTracker SourceSpan and SourceManager integration", "[emitter][line_mapping]") {
    SourceManager sm;
    FileId fid = sm.addBuffer("src/calc.keet", "let x = 1;\nlet y = 2;\nreturn x + y;\n");

    LineDirectiveTracker tracker;

    // Span for line 1
    SourceLocation locStart{fid, 0, 1, 1};
    SourceLocation locEnd{fid, 10, 1, 11};
    SourceSpan span1{locStart, locEnd};

    std::string d1 = tracker.format_line(span1, &sm);
    REQUIRE(d1 == "#line 1 \"src/calc.keet\"\n");

    // Span on same line 1 -> deduplicated
    std::string d2 = tracker.format_line(span1, &sm);
    REQUIRE(d2.empty());

    // Span on line 3
    SourceLocation loc3Start{fid, 22, 3, 1};
    SourceLocation loc3End{fid, 36, 3, 15};
    SourceSpan span3{loc3Start, loc3End};

    std::string d3 = tracker.format_line(span3, &sm);
    REQUIRE(d3 == "#line 3 \"src/calc.keet\"\n");

    // Fallback file when SourceManager is null
    SourceLocation fbStart{INVALID_FILE_ID, 0, 7, 1};
    SourceLocation fbEnd{INVALID_FILE_ID, 0, 7, 5};
    SourceSpan fbSpan{fbStart, fbEnd};

    std::string d4 = tracker.format_line(fbSpan, nullptr, "fallback.keet");
    REQUIRE(d4 == "#line 7 \"fallback.keet\"\n");
}

TEST_CASE("CppEmitter and HeaderEmitter options default to emitting line directives", "[emitter][line_mapping]") {
    CppEmitterOptions cpp_opts;
    REQUIRE(cpp_opts.emit_line_directives);
    REQUIRE(cpp_opts.source_manager == nullptr);
    REQUIRE(cpp_opts.source_file_path.empty());

    CppEmitter cpp_emitter(cpp_opts);
    REQUIRE(cpp_emitter.emits_line_directives());

    cpp_emitter.set_emit_line_directives(false);
    REQUIRE_FALSE(cpp_emitter.emits_line_directives());

    HeaderEmitterOptions hpp_opts;
    REQUIRE(hpp_opts.emit_line_directives);
    REQUIRE(hpp_opts.source_manager == nullptr);
    REQUIRE(hpp_opts.source_file_path.empty());

    HeaderEmitter hpp_emitter(hpp_opts);
    REQUIRE(hpp_emitter.emits_line_directives());

    hpp_emitter.set_emit_line_directives(false);
    REQUIRE_FALSE(hpp_emitter.emits_line_directives());
}
