#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>

using namespace keety::diag;

TEST_CASE("SourceBuffer line offsets and line text", "[diag]") {
    SourceBuffer buf(1, "test.keet", "let a: int = 10;\nlet b: string = \"hello\";\r\nlet c = true;");

    REQUIRE(buf.getId() == 1);
    REQUIRE(buf.getFilePath() == "test.keet");
    REQUIRE(buf.getLineCount() == 3);

    REQUIRE(buf.getLineText(1) == "let a: int = 10;");
    REQUIRE(buf.getLineText(2) == "let b: string = \"hello\";");
    REQUIRE(buf.getLineText(3) == "let c = true;");
    REQUIRE(buf.getLineText(0).empty());
    REQUIRE(buf.getLineText(4).empty());
}

TEST_CASE("SourceBuffer location calculations and UTF-8 handling", "[diag]") {
    // Note: "zażółć" contains UTF-8 characters:
    // 'z' (1 byte), 'a' (1 byte), 'ż' (2 bytes), 'ó' (2 bytes), 'ł' (2 bytes), 'ć' (2 bytes)
    // line 1: "let s = \"zażółć\";\n"
    // line 2: "let x = 42;"
    std::string text = "let s = \"zażółć\";\nlet x = 42;";
    SourceBuffer buf(1, "utf8.keet", text);

    // Start of file
    SourceLocation locStart = buf.getLocationAtOffset(0);
    REQUIRE(locStart.line == 1);
    REQUIRE(locStart.column == 1);
    REQUIRE(locStart.offset == 0);

    // After 'let ' (4 bytes) -> column 5
    SourceLocation locLet = buf.getLocationAtOffset(4);
    REQUIRE(locLet.line == 1);
    REQUIRE(locLet.column == 5);

    // Offset of line 2 start: "let s = \"zażółć\";\n" length in bytes:
    // "let s = \"" is 9 bytes
    // "zażółć" is 1 + 1 + 2 + 2 + 2 + 2 = 10 bytes
    // "\";\n" is 3 bytes -> total 22 bytes. So line 2 starts at byte 22.
    SourceLocation locLine2 = buf.getLocationAtOffset(22);
    REQUIRE(locLine2.line == 2);
    REQUIRE(locLine2.column == 1);
    REQUIRE(locLine2.offset == 22);

    // Column 5 on line 2 ("let ")
    SourceLocation locLine2Col5 = buf.getLocationAtOffset(26);
    REQUIRE(locLine2Col5.line == 2);
    REQUIRE(locLine2Col5.column == 5);

    // Reverse: getOffsetAtLocation
    auto offsetOpt = buf.getOffsetAtLocation(2, 5);
    REQUIRE(offsetOpt.has_value());
    REQUIRE(*offsetOpt == 26);
}

TEST_CASE("SourceBuffer and SourceManager span text extraction", "[diag]") {
    SourceManager sm;
    FileId fid = sm.addBuffer("virtual.keet", "fn calculate(x: int): int {\n    return x * 2;\n}");

    REQUIRE(fid != INVALID_FILE_ID);
    const SourceBuffer* buf = sm.getBuffer(fid);
    REQUIRE(buf != nullptr);
    REQUIRE(sm.getBuffer(999) == nullptr);

    // Span over "calculate": offset 3 to 12
    SourceLocation start{fid, 3, 1, 4};
    SourceLocation end{fid, 12, 1, 13};
    SourceSpan span{start, end};

    REQUIRE(sm.getSpanText(span) == "calculate");
    REQUIRE(sm.getLineText(start) == "fn calculate(x: int): int {");

    // Resolve location
    SourceLocation resolved = sm.resolveLocation(fid, 3);
    REQUIRE(resolved.line == 1);
    REQUIRE(resolved.column == 4);
}
