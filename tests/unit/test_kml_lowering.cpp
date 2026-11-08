#include <catch2/catch_test_macros.hpp>
#include <keety/kml/dependency_analyzer.hpp>
#include <keety/kml/kml_lowerer.hpp>
#include <keety/kml/kml_parser.hpp>
#include <keety/kml/style_scoper.hpp>

using namespace keety::kml;

TEST_CASE("KML Dependency Analyzer - State Runes & Dirty Masks", "[kml][dependency]") {
    std::string_view docSource = R"kml(
<script>
    props: { title: string }
    let mut count = $state(0);
    let mut visible = $state(true);
</script>

<template>
    <box class="wrapper">
        <text>{title}</text>
        <text>Count is: {count}</text>
        {#if visible}
            <button @click={() => { count = count + 1; }}>
                <text>Increment</text>
            </button>
        {/if}
    </box>
</template>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("TestCounter");
    // Add mock prop title
    KmlPropDecl prop;
    prop.name = "title";
    prop.type = "string";
    if (!doc.script) doc.script = KmlScriptBlock{};
    doc.script->props.push_back(prop);

    DependencyAnalyzer analyzer(doc);
    auto analysis = analyzer.analyze();

    // Verify dirty masks
    REQUIRE(analysis.propMasks.count("title") == 1);
    CHECK(analysis.propMasks["title"].bitIndex == 0);
    CHECK(analysis.propMasks["title"].mask == 0x01);

    REQUIRE(analysis.stateMasks.count("count") == 1);
    CHECK(analysis.stateMasks["count"].bitIndex == 1);
    CHECK(analysis.stateMasks["count"].mask == 0x02);

    REQUIRE(analysis.stateMasks.count("visible") == 1);
    CHECK(analysis.stateMasks["visible"].bitIndex == 2);
    CHECK(analysis.stateMasks["visible"].mask == 0x04);

    // Verify expression mask computation
    CHECK(analysis.getDirtyMaskForExpression("title") == 0x01);
    CHECK(analysis.getDirtyMaskForExpression("count + 1") == 0x02);
    CHECK(analysis.getDirtyMaskForExpression("title + count") == 0x03);
    CHECK(analysis.getDirtyMaskForExpression("visible") == 0x04);

    // Verify dependency graph targets
    CHECK(analysis.variableToTargets["count"].size() >= 1);
    CHECK(analysis.variableToTargets["visible"].size() >= 1);
}

TEST_CASE("KML Style Scoper - Web and TUI", "[kml][style]") {
    std::string_view docSource = R"kml(
<template>
    <box class="btn"><text>Click</text></box>
</template>
<style>
    .btn {
        background-color: blue;
        padding: 1;
    }
</style>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("MyButton");

    StyleScoper scoper(doc);
    auto hash = scoper.generateScopeHash();
    CHECK(hash.starts_with("kml-"));

    auto webRes = scoper.lower(StyleTarget::Web);
    CHECK(webRes.scopeClass == hash);
    CHECK(webRes.scopedCss.find(".btn." + hash) != std::string::npos);

    auto tuiRes = scoper.lower(StyleTarget::Tui);
    CHECK(tuiRes.tuiStyleInitializers.find("MyButton") != std::string::npos);
}

TEST_CASE("KML Lowerer - Two-Way Binding & Modifiers Desugaring", "[kml][lowerer]") {
    // Test desugarTwoWayBinding
    KmlDirective dir;
    dir.name = "bind";
    dir.targetProperty = "value";

    std::string desugared = KmlLowerer::desugarTwoWayBinding(dir, "username");
    CHECK(desugared.find("prop: value={username}") != std::string::npos);
    CHECK(desugared.find("onValueChange={(val) => { username = val; }}") != std::string::npos);

    // Test desugarEventModifiers
    std::vector<std::string> mods = {"debounce(200)", "prevent"};
    std::string wrapped = KmlLowerer::desugarEventModifiers("submitForm", mods);
    CHECK(wrapped == "std.ui.withPreventDefault(std.ui.debounce(submitForm, 200))");
}

TEST_CASE("KML Lowerer - Full Component Synthesis", "[kml][lowerer]") {
    std::string_view docSource = R"kml(
<script>
    let mut count = $state(0);
</script>

<template>
    <box class="counter">
        <text>Value: {count}</text>
        <button @click={() => { count = count + 1; }}>
            <text>Add</text>
        </button>
    </box>
</template>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("CounterApp");

    KmlLowerer lowerer(doc);
    auto res = lowerer.lower();

    CHECK(res.componentName == "CounterApp");
    CHECK(!res.scopeClass.empty());

    // Check generated KeetScript contents
    const std::string& code = res.generatedKeetScript;
    CHECK(code.find("export class CounterApp inherits std.ui.Component") != std::string::npos);
    CHECK(code.find("const DIRTY_STATE_count: int = 0x") != std::string::npos);
    CHECK(code.find("const DIRTY_ALL: int = 0xFFFFFFFF;") != std::string::npos);
    CHECK(code.find("pub fn constructor(): void") != std::string::npos);
    CHECK(code.find("pub fn mount(): std.ui.UINode") != std::string::npos);
    CHECK(code.find("pub fn update(dirtyMask: int): void") != std::string::npos);
    CHECK(code.find("pub fn renderVdom(): std.ui.VNode") != std::string::npos);
    CHECK(code.find("pub fn destroy(): void") != std::string::npos);
}

