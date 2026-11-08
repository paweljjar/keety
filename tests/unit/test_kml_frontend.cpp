#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_location.hpp>
#include <keety/kml/kml_lexer.hpp>
#include <keety/kml/kml_parser.hpp>

using namespace keety::kml;
using namespace keety::diag;

TEST_CASE("KML Lexer - SFC Root Tokens", "[kml][lexer]") {
    std::string_view sfcSource = R"kml(
        <script>
            let mut count = $state(0);
        </script>
        <template>
            <box class="container">
                <text>Hello World</text>
            </box>
        </template>
        <style>
            .container { padding: 1; }
        </style>
    )kml";

    KmlLexer lexer(sfcSource, 1);
    lexer.setMode(LexerMode::SfcRoot);

    auto tok1 = lexer.nextToken();
    CHECK(tok1.kind == KmlTokenKind::TagScriptOpen);

    keety::diag::SourceSpan scriptSpan;
    std::string scriptBody = lexer.lexRawUntilClosingTag("</script>", scriptSpan);
    CHECK(scriptBody.find("$state(0)") != std::string::npos);

    auto tok2 = lexer.nextToken();
    CHECK(tok2.kind == KmlTokenKind::TagScriptClose);

    auto tok3 = lexer.nextToken();
    CHECK(tok3.kind == KmlTokenKind::TagTemplateOpen);
}

TEST_CASE("KML Lexer - Balanced Expressions and Directives", "[kml][lexer]") {
    std::string_view tmpl = R"kml(
        <button @click={handleClick} @keydown.enter={submit} $bind.value={myState}>
            {count + 1}
        </button>
    )kml";

    KmlLexer lexer(tmpl, 1);
    lexer.setMode(LexerMode::TemplateContent);

    auto tok1 = lexer.nextToken(); // TextRun (whitespace)
    auto tok2 = lexer.nextToken(); // TagOpenStart ("button")
    CHECK(tok2.kind == KmlTokenKind::TagOpenStart);
    CHECK(tok2.text == "button");

    lexer.setMode(LexerMode::TagAttributes);
    auto atClick = lexer.nextToken();
    CHECK(atClick.kind == KmlTokenKind::DirectiveEvent);
    CHECK(atClick.text == "click");

    auto eq1 = lexer.nextToken();
    CHECK(eq1.kind == KmlTokenKind::Equals);

    auto expr1 = lexer.nextToken();
    CHECK(expr1.kind == KmlTokenKind::ExprBlock);
    CHECK(expr1.text == "handleClick");

    auto atKeydown = lexer.nextToken();
    CHECK(atKeydown.kind == KmlTokenKind::DirectiveEvent);
    CHECK(atKeydown.text == "keydown");

    auto modEnter = lexer.nextToken();
    CHECK(modEnter.kind == KmlTokenKind::DirectiveModifier);
    CHECK(modEnter.text == "enter");

    auto eq2 = lexer.nextToken();
    CHECK(eq2.kind == KmlTokenKind::Equals);

    auto expr2 = lexer.nextToken();
    CHECK(expr2.kind == KmlTokenKind::ExprBlock);
    CHECK(expr2.text == "submit");

    auto bindVal = lexer.nextToken();
    CHECK(bindVal.kind == KmlTokenKind::DirectiveBind);
    CHECK(bindVal.text == "bind.value");
}

TEST_CASE("KML Parser - Complete SFC Document", "[kml][parser]") {
    std::string_view docSource = R"kml(
<script>
    let mut count = $state(0);
    let doubleCount = $derived(count * 2);
    $effect(() => {
        print("Count changed:", count);
    });
</script>

<template>
    <box class="counter-box">
        <text>Count: {count}</text>
        <button @click={() => { count = count + 1; }}>
            <text>Increment</text>
        </button>
        {#if count > 5}
            <text>High count!</text>
        {:else if count > 0}
            <text>Positive count</text>
        {:else}
            <text>Zero or negative</text>
        {/if}
        {#for (item, idx in items; key=item.id)}
            <text>{idx}: {item.name}</text>
        {/for}
    </box>
</template>

<style>
    .counter-box {
        border-color: blue;
        padding: 2;
    }
</style>
    )kml";

    KmlParser parser(docSource, 1);
    KmlDocument doc = parser.parseDocument("CounterComponent");

    REQUIRE(doc.componentName == "CounterComponent");
    REQUIRE(doc.script.has_value());
    CHECK(doc.script->runes.size() == 3);
    CHECK(doc.script->runes[0].kind == KmlStateRune::Kind::State);
    CHECK(doc.script->runes[0].variableName == "count");
    CHECK(doc.script->runes[1].kind == KmlStateRune::Kind::Derived);
    CHECK(doc.script->runes[1].variableName == "doubleCount");
    CHECK(doc.script->runes[2].kind == KmlStateRune::Kind::Effect);

    REQUIRE(doc.style.has_value());

    REQUIRE(!doc.templateNodes.empty());
    // Find the root box element
    KmlElementNode* boxElem = nullptr;
    for (const auto& node : doc.templateNodes) {
        if (node->getKind() == KmlNodeKind::Element) {
            boxElem = static_cast<KmlElementNode*>(node.get());
            break;
        }
    }
    REQUIRE(boxElem != nullptr);
    CHECK(boxElem->getTagName() == "box");
    CHECK(boxElem->getAttributes().size() == 1);
    CHECK(boxElem->getAttributes()[0].name == "class");
    CHECK(boxElem->getAttributes()[0].value == "counter-box");

    // Children inside box: <text>, <button>, {#if}, {#for}
    bool foundIf = false;
    bool foundFor = false;
    bool foundButton = false;

    for (const auto& child : boxElem->getChildren()) {
        if (child->getKind() == KmlNodeKind::IfBlock) {
            foundIf = true;
            auto* ifBlock = static_cast<KmlIfBlock*>(child.get());
            CHECK(ifBlock->getCondition() == "count > 5");
            CHECK(ifBlock->getElseIfBranches().size() == 1);
            CHECK(ifBlock->getElseIfBranches()[0].condition == "count > 0");
            CHECK(!ifBlock->getElseBranch().empty());
        } else if (child->getKind() == KmlNodeKind::ForBlock) {
            foundFor = true;
            auto* forBlock = static_cast<KmlForBlock*>(child.get());
            CHECK(forBlock->getItemVar() == "item");
            CHECK(forBlock->getIndexVar().value_or("") == "idx");
            CHECK(forBlock->getIterableExpr() == "items");
            CHECK(forBlock->getKeyExpr().value_or("") == "item.id");
        } else if (child->getKind() == KmlNodeKind::Element) {
            auto* elem = static_cast<KmlElementNode*>(child.get());
            if (elem->getTagName() == "button") {
                foundButton = true;
                CHECK(!elem->getDirectives().empty());
                CHECK(elem->getDirectives()[0].name == "click");
            }
        }
    }

    CHECK(foundIf);
    CHECK(foundFor);
    CHECK(foundButton);
}
