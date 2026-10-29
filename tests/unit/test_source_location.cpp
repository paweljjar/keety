#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_location.hpp>

using namespace keety::diag;

TEST_CASE("SourceLocation default and validity", "[diag]") {
    SourceLocation loc;
    REQUIRE_FALSE(loc.isValid());
    REQUIRE(loc.fileId == INVALID_FILE_ID);
    REQUIRE(loc.offset == 0);
    REQUIRE(loc.line == 1);
    REQUIRE(loc.column == 1);

    SourceLocation validLoc{1, 42, 3, 10};
    REQUIRE(validLoc.isValid());
    REQUIRE(validLoc.fileId == 1);
    REQUIRE(validLoc.offset == 42);
    REQUIRE(validLoc.line == 3);
    REQUIRE(validLoc.column == 10);
}

TEST_CASE("SourceLocation comparisons", "[diag]") {
    SourceLocation l1{1, 10, 2, 5};
    SourceLocation l2{1, 10, 2, 5};
    SourceLocation l3{1, 15, 2, 10};
    SourceLocation l4{2, 5, 1, 5};

    REQUIRE(l1 == l2);
    REQUIRE(l1 < l3);
    REQUIRE(l1 < l4);
}

TEST_CASE("SourceSpan validity and properties", "[diag]") {
    SourceLocation start{1, 10, 2, 1};
    SourceLocation end{1, 25, 2, 16};
    SourceSpan span{start, end};

    REQUIRE(span.isValid());
    REQUIRE(span.getFileId() == 1);
    REQUIRE(span.length() == 15);

    SourceLocation inside{1, 15, 2, 6};
    SourceLocation before{1, 5, 1, 5};
    SourceLocation after{1, 25, 2, 16};
    SourceLocation diffFile{2, 15, 2, 6};

    REQUIRE(span.contains(inside));
    REQUIRE_FALSE(span.contains(before));
    REQUIRE_FALSE(span.contains(after)); // [start, end) is half-open
    REQUIRE_FALSE(span.contains(diffFile));
}

TEST_CASE("SourceSpan merging", "[diag]") {
    SourceSpan span1{SourceLocation{1, 10, 2, 1}, SourceLocation{1, 20, 2, 11}};
    SourceSpan span2{SourceLocation{1, 25, 3, 1}, SourceLocation{1, 35, 3, 11}};

    SourceSpan merged = span1.merge(span2);
    REQUIRE(merged.isValid());
    REQUIRE(merged.start == span1.start);
    REQUIRE(merged.end == span2.end);
    REQUIRE(merged.length() == 25);

    // Merging with invalid returns the valid span
    SourceSpan invalidSpan;
    REQUIRE(span1.merge(invalidSpan) == span1);
    REQUIRE(invalidSpan.merge(span1) == span1);
}
