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

TEST_CASE("KML Developer Tooling - LSP Hover on Primitives, Runes, Directives, and Script Symbols", "[kml][lsp][hover]") {
    lsp::Analyzer analyzer;
    std::string kmlCode =
        "<script>\n"
        "    import math;\n"
        "    import \"./Task.kml\";\n"
        "    let tasks = $state([1, 2, 3]);\n"
        "    let mut newTaskTitle = $state(\"\");\n"
        "    fn addTask(): void {}\n"
        "</script>\n"
        "<template>\n"
        "    <vstack gap={12} padding={16}>\n"
        "        <text color=\"#ffffff\">My Tasks</text>\n"
        "        <input $bind.value={newTaskTitle} placeholder=\"New task...\" />\n"
        "        <button @click={addTask}><text>Add</text></button>\n"
        "        <Task />\n"
        "    </vstack>\n"
        "</template>\n";
    lsp::Document doc("file:///project/App.kml", "kml", 1, kmlCode);

    // 1. Hover on '$state' rune in script (line 3, col 18)
    auto hoverState = analyzer.getHover(doc, lsp::Position{3, 18});
    REQUIRE(hoverState.has_value());
    CHECK(hoverState->contents.value.find("KML State Rune") != std::string::npos);

    // 2. Hover on declared state variable 'tasks' (line 3, col 9)
    auto hoverTasks = analyzer.getHover(doc, lsp::Position{3, 9});
    REQUIRE(hoverTasks.has_value());
    CHECK(hoverTasks->contents.value.find("let mut tasks = $state") != std::string::npos);

    // 3. Hover on 'math' import in script (line 1, col 12)
    auto hoverMath = analyzer.getHover(doc, lsp::Position{1, 12});
    REQUIRE(hoverMath.has_value());
    CHECK(hoverMath->contents.value.find("Standard Library Module: `math`") != std::string::npos);

    // 4. Hover on '<vstack>' tag (line 8, col 7)
    auto hoverVstack = analyzer.getHover(doc, lsp::Position{8, 7});
    REQUIRE(hoverVstack.has_value());
    CHECK(hoverVstack->contents.value.find("Vertical Stack Layout Container") != std::string::npos);

    // 5. Hover on layout attribute 'gap' (line 8, col 13)
    auto hoverGap = analyzer.getHover(doc, lsp::Position{8, 13});
    REQUIRE(hoverGap.has_value());
    CHECK(hoverGap->contents.value.find("gap") != std::string::npos);

    // 6. Hover on '<input>' tag (line 10, col 10)
    auto hoverInput = analyzer.getHover(doc, lsp::Position{10, 10});
    REQUIRE(hoverInput.has_value());
    CHECK(hoverInput->contents.value.find("Text Input Field") != std::string::npos);

    // 7. Hover on '$bind.value' directive (line 10, col 18)
    auto hoverBind = analyzer.getHover(doc, lsp::Position{10, 18});
    REQUIRE(hoverBind.has_value());
    CHECK(hoverBind->contents.value.find("Two-Way Binding") != std::string::npos);

    // 8. Hover on template-bound variable 'newTaskTitle' inside {newTaskTitle} (line 10, col 30)
    auto hoverBoundVar = analyzer.getHover(doc, lsp::Position{10, 30});
    REQUIRE(hoverBoundVar.has_value());
    CHECK(hoverBoundVar->contents.value.find("newTaskTitle") != std::string::npos);

    // 9. Hover on '@click' directive (line 11, col 18)
    auto hoverClick = analyzer.getHover(doc, lsp::Position{11, 18});
    REQUIRE(hoverClick.has_value());
    CHECK(hoverClick->contents.value.find("Event Directive") != std::string::npos);

    // 10. Hover on template-bound function 'addTask' (line 11, col 26)
    auto hoverBoundFn = analyzer.getHover(doc, lsp::Position{11, 26});
    REQUIRE(hoverBoundFn.has_value());
    CHECK(hoverBoundFn->contents.value.find("fn addTask") != std::string::npos);

    // 11. Hover on custom component '<Task />' (line 12, col 11)
    auto hoverTaskComp = analyzer.getHover(doc, lsp::Position{12, 11});
    REQUIRE(hoverTaskComp.has_value());
    CHECK(hoverTaskComp->contents.value.find("component Task") != std::string::npos);
}

TEST_CASE("KML Developer Tooling - LSP Go-to-Definition on Components and Template Expressions", "[kml][lsp][definition]") {
    lsp::Analyzer analyzer;
    std::string kmlCode =
        "<script>\n"
        "    import \"./Task.kml\";\n"
        "    let mut newTaskTitle = $state(\"\");\n"
        "    fn addTask(): void {}\n"
        "</script>\n"
        "<template>\n"
        "    <vstack>\n"
        "        <input $bind.value={newTaskTitle} />\n"
        "        <button @click={addTask}><text>Add</text></button>\n"
        "    </vstack>\n"
        "</template>\n";
    lsp::Document doc("file:///project/App.kml", "kml", 1, kmlCode);

    // 1. Definition jump on template expression 'newTaskTitle' (line 7, col 30) -> line 2 in script
    auto defVar = analyzer.findDefinition(doc, lsp::Position{7, 30});
    REQUIRE(defVar.has_value());
    CHECK(defVar->uri == "file:///project/App.kml");
    CHECK(defVar->range.start.line == 2);

    // 2. Definition jump on template expression 'addTask' (line 8, col 26) -> line 3 in script
    auto defFn = analyzer.findDefinition(doc, lsp::Position{8, 26});
    REQUIRE(defFn.has_value());
    CHECK(defFn->uri == "file:///project/App.kml");
    CHECK(defFn->range.start.line == 3);
}

TEST_CASE("KML Developer Tooling - LSP Completions for stdlib math. inside <script>", "[kml][lsp][completion]") {
    lsp::Analyzer analyzer;
    std::string kmlCode =
        "<script>\n"
        "    import math;\n"
        "    math.\n"
        "</script>\n";
    lsp::Document doc("file:///project/App.kml", "kml", 1, kmlCode);

    // Dot completion on 'math.' (line 2, col 9)
    auto completions = analyzer.getCompletions(doc, lsp::Position{2, 9});
    bool foundSin = false;
    bool foundCos = false;
    bool foundSqrt = false;
    bool foundPi = false;

    for (const auto& item : completions.items) {
        if (item.label == "sin") foundSin = true;
        else if (item.label == "cos") foundCos = true;
        else if (item.label == "sqrt") foundSqrt = true;
        else if (item.label == "PI") foundPi = true;
    }

    CHECK(foundSin);
    CHECK(foundCos);
    CHECK(foundSqrt);
    CHECK(foundPi);
}
