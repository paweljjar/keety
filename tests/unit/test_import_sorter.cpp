#include <catch2/catch_test_macros.hpp>
#include <keety/fmt/import_sorter.hpp>

using namespace keety;
using namespace keety::fmt;

TEST_CASE("ImportSorter classifies import tiers correctly", "[fmt][import]") {
    CHECK(ImportSorter::classify("io") == ImportTier::StandardLibrary);
    CHECK(ImportSorter::classify("fs") == ImportTier::StandardLibrary);
    CHECK(ImportSorter::classify("net.http") == ImportTier::StandardLibrary);
    CHECK(ImportSorter::classify("time") == ImportTier::StandardLibrary);
    CHECK(ImportSorter::classify("crypto") == ImportTier::StandardLibrary);
    CHECK(ImportSorter::classify("test") == ImportTier::StandardLibrary);

    CHECK(ImportSorter::classify("github.com/org/pkg") == ImportTier::ThirdParty);
    CHECK(ImportSorter::classify("external_lib") == ImportTier::ThirdParty);

    CHECK(ImportSorter::classify("./utils") == ImportTier::LocalRelative);
    CHECK(ImportSorter::classify("../core/types") == ImportTier::LocalRelative);
}

TEST_CASE("ImportSorter sorts and groups imports into 3 distinct tiers alphabetically", "[fmt][import]") {
    diag::SourceSpan dummySpan{};

    ast::ImportDecl impRelative1(dummySpan, "./utils");
    ast::ImportDecl impRelative2(dummySpan, "../common");
    ast::ImportDecl impStdlib1(dummySpan, "time");
    ast::ImportDecl impStdlib2(dummySpan, "io");
    ast::ImportDecl impStdlib3(dummySpan, "fs");
    ast::ImportDecl impThirdParty1(dummySpan, "github.com/zlib/zlib");
    ast::ImportDecl impThirdParty2(dummySpan, "github.com/async/channel");

    std::vector<const ast::ImportDecl*> input = {
        &impRelative1,
        &impStdlib1,
        &impThirdParty1,
        &impRelative2,
        &impStdlib2,
        &impThirdParty2,
        &impStdlib3
    };

    auto groups = ImportSorter::groupAndSort(input);
    REQUIRE(groups.size() == 3);

    // Group 1: Standard Library (fs, io, time)
    const auto& stdlibGroup = groups[0];
    REQUIRE(stdlibGroup.size() == 3);
    CHECK(stdlibGroup[0]->getModulePath() == "fs");
    CHECK(stdlibGroup[1]->getModulePath() == "io");
    CHECK(stdlibGroup[2]->getModulePath() == "time");

    // Group 2: Third-Party (github.com/async/channel, github.com/zlib/zlib)
    const auto& thirdPartyGroup = groups[1];
    REQUIRE(thirdPartyGroup.size() == 2);
    CHECK(thirdPartyGroup[0]->getModulePath() == "github.com/async/channel");
    CHECK(thirdPartyGroup[1]->getModulePath() == "github.com/zlib/zlib");

    // Group 3: Local Relative (../common, ./utils)
    const auto& localGroup = groups[2];
    REQUIRE(localGroup.size() == 2);
    CHECK(localGroup[0]->getModulePath() == "../common");
    CHECK(localGroup[1]->getModulePath() == "./utils");
}
