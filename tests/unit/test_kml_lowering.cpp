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
    CHECK(wrapped == "ui.withPreventDefault(ui.debounce(submitForm, 200))");
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
    CHECK(code.find("export class CounterApp inherits ui.Component") != std::string::npos);
    CHECK(code.find("const DIRTY_STATE_count: int = 0x") != std::string::npos);
    CHECK(code.find("const DIRTY_ALL: int = 0xFFFFFFFF;") != std::string::npos);
    CHECK(code.find("pub fn constructor(): void") != std::string::npos);
    CHECK(code.find("pub fn mount(): ui.UINode") != std::string::npos);
    CHECK(code.find("pub fn update(dirtyMask: int): void") != std::string::npos);
    CHECK(code.find("pub fn renderVdom(): ui.VNode") != std::string::npos);
    CHECK(code.find("pub fn destroy(): void") != std::string::npos);
}

TEST_CASE("KML Lowerer - Desktop Layout Tags, Attributes and Input Binding Lowering", "[kml][lowerer][desktop]") {
    std::string_view docSource = R"kml(
<script>
    let mut message = $state("hello");
    let mut messages = $state([]);

    fn sendMessage(): void {
        messages = messages + [message];
        message = "";
    }
</script>

<template>
    <vstack width="800" height="100%" bg="#1e1e2e" padding="16" gap="12">
        <hstack justify="space-between" align="center" padding="8" bg="#181825" radius="6">
            <text color="#cdd6f4" font-size="16">KeetChat #general</text>
        </hstack>

        <scroll grow="1" bg="#11111b" padding="12" radius="8">
            <vstack gap="8">
                <box class="msg" padding="6" bg="#313244" radius="4">
                    <text color="#cdd6f4">{message}</text>
                </box>
            </vstack>
        </scroll>

        <hstack gap="8" align="center">
            <input $bind.value={message} @keydown.enter={sendMessage} grow="1" height="36" bg="#181825" color="#cdd6f4" radius="6" padding="8" />
            <button @click={sendMessage} width="100" height="36" bg="#89b4fa" radius="6">
                <text color="#11111b">Send</text>
            </button>
        </hstack>
    </vstack>
</template>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("ChatApp");

    KmlLowerer lowerer(doc);
    auto res = lowerer.lower();

    CHECK(res.componentName == "ChatApp");
    const std::string& cpp = res.generatedCppCode;

    // 1. Verify layout tags lowering
    CHECK(cpp.find("std::make_shared<::keet::ui::StackNode>(::keet::ui::FlexDirection::Column)") != std::string::npos);
    CHECK(cpp.find("std::make_shared<::keet::ui::StackNode>(::keet::ui::FlexDirection::Row)") != std::string::npos);
    CHECK(cpp.find("std::make_shared<::keet::ui::ScrollNode>()") != std::string::npos);
    CHECK(cpp.find("getStyle().overflowScrollY = true") != std::string::npos);
    CHECK(cpp.find("std::make_shared<::keet::ui::InputNode>()") != std::string::npos);

    // 2. Verify layout attributes lowering
    CHECK(cpp.find("getStyle().width = ::keet::ui::Dimension::Pixels(800)") != std::string::npos);
    CHECK(cpp.find("getStyle().height = ::keet::ui::Dimension::Percent(100)") != std::string::npos);
    CHECK(cpp.find("getStyle().padding = ::keet::ui::Edges::All(16)") != std::string::npos);
    CHECK(cpp.find("getStyle().gap = 12") != std::string::npos);
    CHECK(cpp.find("getStyle().justifyContent = ::keet::ui::JustifyContent::SpaceBetween") != std::string::npos);
    CHECK(cpp.find("getStyle().alignItems = ::keet::ui::AlignItems::Center") != std::string::npos);
    CHECK(cpp.find("getStyle().flexGrow = 1") != std::string::npos);
    CHECK(cpp.find("getStyle().borderRadius = 8") != std::string::npos);
    CHECK(cpp.find("getStyle().fontSize = 16") != std::string::npos);
    CHECK(cpp.find("getStyle().backgroundColorRgba = ::keet::ui::RgbaColor::parseHex(\"#1e1e2e\")") != std::string::npos);
    CHECK(cpp.find("getStyle().textColorRgba = ::keet::ui::RgbaColor::parseHex(\"#cdd6f4\")") != std::string::npos);

    // 3. Verify $bind.value lowering
    CHECK(cpp.find("setValueSilent(::keet::to_string(this->message))") != std::string::npos);
    CHECK(cpp.find("this->message = val;") != std::string::npos);

    // 4. Verify @keydown.enter lowering
    CHECK(cpp.find("if (key == \"Enter\")") != std::string::npos);
    CHECK(cpp.find("this->sendMessage();") != std::string::npos);

    // 5. Verify KeetScript mount lowering for stack and scroll
    const std::string& ks = res.generatedKeetScript;
    CHECK(ks.find("new ui.StackNode()") != std::string::npos);
    CHECK(ks.find("style.flexDirection = ui.FlexDirection.Column") != std::string::npos);
    CHECK(ks.find("style.flexDirection = ui.FlexDirection.Row") != std::string::npos);
    CHECK(ks.find("new ui.ScrollNode()") != std::string::npos);
    CHECK(ks.find("style.overflowScrollY = true") != std::string::npos);
}

TEST_CASE("KML Lowerer - Standardized script imports for both .kml and .keet", "[kml][lowerer][imports]") {
    std::string_view docSource = R"kml(
<script>
    import "./components/Task.kml";
    import "./utils.keet";
    from io import println;

    let title = $state("My App");
</script>

<template>
    <vstack>
        <Task title={title} />
        <OtherComp />
    </vstack>
</template>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("AppWithImports");

    KmlLowerer lowerer(doc);
    auto res = lowerer.lower();

    // 1. Interface KeetScript must preserve explicit script imports and only fallback for unimported components
    const std::string& iface = res.generatedInterfaceKeet;
    CHECK(iface.find("import ui;") != std::string::npos);
    CHECK(iface.find("import \"./components/Task.kml\";") != std::string::npos);
    CHECK(iface.find("import \"./utils.keet\";") != std::string::npos);
    CHECK(iface.find("from io import println;") != std::string::npos);
    // OtherComp is not in script, so it falls back to ./OtherComp.kml
    CHECK(iface.find("import \"./OtherComp.kml\";") != std::string::npos);
    // Task was explicitly imported, so there must NOT be a duplicate "./Task.kml" fallback
    CHECK(iface.find("import \"./Task.kml\";") == std::string::npos);

    // 2. Generated C++ Header must include headers for components, keet modules and stdlib
    const std::string& hpp = res.generatedHppCode;
    CHECK(hpp.find("#include \"components/Task.hpp\"") != std::string::npos);
    CHECK(hpp.find("#include \"utils.hpp\"") != std::string::npos);
    CHECK(hpp.find("#include <keet/std/io.hpp>") != std::string::npos);
    CHECK(hpp.find("#include \"OtherComp.hpp\"") != std::string::npos);
}



