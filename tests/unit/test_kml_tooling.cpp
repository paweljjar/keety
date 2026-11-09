#include <catch2/catch_test_macros.hpp>
#include <keety/fmt/formatter.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>

using namespace keety;

TEST_CASE("KML Developer Tooling - LSP Completions for Runes & Primitives", "[kml][lsp]") {
    lsp::Analyzer analyzer;
    lsp::Document doc("file:///project/Counter.kml", "kml", 1, R"kml(
<script>
    $
</script>
<template>
    <
</template>
    )kml");

    auto completions = analyzer.getCompletions(doc, lsp::Position{2, 5});
    bool foundState = false;
    bool foundDerived = false;
    bool foundEffect = false;
    bool foundBox = false;

    for (const auto& item : completions.items) {
        if (item.label == "$state") foundState = true;
        else if (item.label == "$derived") foundDerived = true;
        else if (item.label == "$effect") foundEffect = true;
        else if (item.label == "box") foundBox = true;
    }

    CHECK(foundState);
    CHECK(foundDerived);
    CHECK(foundEffect);
    CHECK(foundBox);
}

TEST_CASE("KML Developer Tooling - Formatter Support", "[kml][fmt]") {
    fmt::Formatter formatter;
    std::string unformatted = R"kml(<script>
let x:int=10;
fn test():void{
let y:int=20;
}
</script>
<template>
<box>
<text>Hello</text>
</box>
</template>)kml";

    std::string formatted = formatter.format(unformatted, "MyComp.kml");
    CHECK(formatted.find("let x: int = 10;") != std::string::npos);
    CHECK(formatted.find("fn test(): void {") != std::string::npos);
    CHECK(formatted.find("<script>") != std::string::npos);
    CHECK(formatted.find("</script>") != std::string::npos);
}

TEST_CASE("KML Developer Tooling - Linter Hygiene Rules", "[kml][lint]") {
    lint::LintEngine engine;
    engine.registerDefaultRules();

    std::string kmlCode = R"kml(
<script>
    let col = Color::Blue;
</script>
<template>
    <box>
        {#for item in items}
            <button :click={handleClick}>Click</button>
        {/for}
    </box>
</template>
    )kml";

    auto diags = engine.lintSource(kmlCode, "TestComponent.kml");

    bool foundUnkeyedFor = false;
    bool foundColonColon = false;
    bool foundBadDirective = false;

    for (const auto& d : diags) {
        if (d.ruleCode == "L030") foundUnkeyedFor = true;
        if (d.ruleCode == "L031") foundColonColon = true;
        if (d.ruleCode == "L032") foundBadDirective = true;
    }

    CHECK(foundUnkeyedFor);
    CHECK(foundColonColon);
    CHECK(foundBadDirective);
}

TEST_CASE("KML Developer Tooling - LSP Analyzer Diagnostics on Valid KML", "[kml][lsp]") {
    lsp::Analyzer analyzer;
    lsp::Document doc("file:///project/Counter.kml", "kml", 1, R"kml(<script>
    let mut count = $state(0);
    let doubleCount = $derived(count * 2);

    fn increment(): void {
        count = count + 1;
    }
</script>

<template>
    <box class="counter-card">
        <text class="count-display">Current Count: {count} (Doubled: {doubleCount})</text>
        <button @click={increment}><text>+ Increment</text></button>
    </box>
</template>
)kml");

    auto diags = analyzer.analyze(doc);
    CHECK(diags.empty());
}
