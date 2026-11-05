#include <catch2/catch_test_macros.hpp>
#include <keety/fmt/formatter.hpp>

using namespace keety;
using namespace keety::fmt;

TEST_CASE("Formatter enforces 4-space indentation and 1TBS bracing", "[fmt][formatter]") {
    std::string_view unformatted =
        "fn calculate(a:int,b:int):int{\n"
        "  if(a>b){\n"
        "      return a-b;\n"
        "  }\n"
        "  else{\n"
        "      return b-a;\n"
        "  }\n"
        "}";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    std::string expected =
        "fn calculate(a: int, b: int): int {\n"
        "    if (a > b) {\n"
        "        return a - b;\n"
        "    } else {\n"
        "        return b - a;\n"
        "    }\n"
        "}\n";

    CHECK(formatted == expected);
    // Idempotency: fmt(fmt(x)) == fmt(x)
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter organizes and sorts imports into canonical 3-tier structure", "[fmt][formatter]") {
    std::string_view unformatted =
        "import \"./util\";\n"
        "import time;\n"
        "import \"github.com/org/pkg\";\n"
        "import io;\n"
        "import \"../common\";\n"
        "\n"
        "fn main(): int {\n"
        "    return 0;\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    std::string expected =
        "import io;\n"
        "import time;\n"
        "\n"
        "import \"github.com/org/pkg\";\n"
        "\n"
        "import \"../common\";\n"
        "import \"./util\";\n"
        "\n"
        "fn main(): int {\n"
        "    return 0;\n"
        "}\n";

    CHECK(formatted == expected);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter preserves attached comments and vertical spacing", "[fmt][formatter]") {
    std::string_view unformatted =
        "// Module license\n"
        "\n"
        "import io;\n"
        "\n"
        "// Adds two numbers\n"
        "fn add(a: int, b: int): int {\n"
        "    // Step 1\n"
        "    let sum: int = a + b; // inline sum\n"
        "    return sum;\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    std::string expected =
        "// Module license\n"
        "\n"
        "import io;\n"
        "\n"
        "// Adds two numbers\n"
        "fn add(a: int, b: int): int {\n"
        "    // Step 1\n"
        "    let sum: int = a + b; // inline sum\n"
        "    return sum;\n"
        "}\n";

    CHECK(formatted == expected);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats classes, methods, and member fields", "[fmt][formatter]") {
    std::string_view unformatted =
        "class Player inherits Entity {\n"
        "pub let id:int=1;\n"
        "pub let name:string;\n"
        "fn isAlive():bool{\n"
        "return this.id>0;\n"
        "}\n"
        "}";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    std::string expected =
        "class Player inherits Entity {\n"
        "    pub let id: int = 1;\n"
        "    pub let name: string;\n"
        "\n"
        "    fn isAlive(): bool {\n"
        "        return this.id > 0;\n"
        "    }\n"
        "}\n";

    CHECK(formatted == expected);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats enums with explicit values and trailing commas", "[fmt][formatter]") {
    std::string_view unformatted =
        "enum Status: int {\n"
        "Ok = 200,\n"
        "Created=201,\n"
        "NotFound = 404\n"
        "}";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    std::string expected =
        "enum Status: int {\n"
        "    Ok = 200,\n"
        "    Created = 201,\n"
        "    NotFound = 404,\n"
        "}\n";

    CHECK(formatted == expected);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats loops with labels and init statements", "[fmt][formatter]") {
    std::string_view unformatted =
        "fn testLoops(): void {\n"
        "    while (let i: int = 0; i < 10) as loopW {\n"
        "        i += 1;\n"
        "        if (i == 5) {\n"
        "            break loopW;\n"
        "        }\n"
        "    }\n"
        "    for (let k: int = 0; k < 5; k += 1) as loopF {\n"
        "        continue loopF;\n"
        "    }\n"
        "    for (const item in items) as loopIn {\n"
        "        process(item);\n"
        "    }\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    CHECK(formatted == unformatted);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats pattern matching switch statements", "[fmt][formatter]") {
    std::string_view unformatted =
        "fn eval(code: int): string {\n"
        "    switch (code) {\n"
        "        case (1, 2) {\n"
        "            return \"low\";\n"
        "        }\n"
        "        case (3) {\n"
        "            return \"mid\";\n"
        "        }\n"
        "        default {\n"
        "            return \"high\";\n"
        "        }\n"
        "    }\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    CHECK(formatted == unformatted);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats zonk fallback and recovery blocks", "[fmt][formatter]") {
    std::string_view unformatted =
        "fn testZonk(): int {\n"
        "    let val1: int = parseNumber(\"123\") zonk (0);\n"
        "    let val2: int = parseNumber(\"abc\") zonk (err) {\n"
        "        yield -1;\n"
        "    };\n"
        "    return val1 + val2;\n"
        "}\n";

    std::string_view expected =
        "fn testZonk(): int {\n"
        "    let val1: int = parseNumber(\"123\") zonk 0;\n"
        "    let val2: int = parseNumber(\"abc\") zonk (err) {\n"
        "        yield -1;\n"
        "    };\n"
        "    return val1 + val2;\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    CHECK(formatted == expected);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter formats closures and template literals", "[fmt][formatter]") {
    std::string_view unformatted =
        "fn testAdvanced(): void {\n"
        "    let doubleFn: (x: int) => int = fn(x: int): int {\n"
        "        return x * 2;\n"
        "    };\n"
        "    let msg: string = `Count is ${doubleFn(10)}!`;\n"
        "}\n";

    Formatter formatter;
    std::string formatted = formatter.format(unformatted);

    CHECK(formatted == unformatted);
    CHECK(formatter.format(formatted) == formatted);
}

TEST_CASE("Formatter generates clean unified diffs", "[fmt][diff]") {
    std::string_view original = "fn test():void{\nreturn 1;\n}\n";
    std::string_view formatted = "fn test(): void {\n    return 1;\n}\n";

    std::string diff = Formatter::generateDiff(original, formatted, "main.keet");
    REQUIRE(!diff.empty());
    CHECK(diff.find("--- a/main.keet") != std::string::npos);
    CHECK(diff.find("+++ b/main.keet") != std::string::npos);
    CHECK(diff.find("-fn test():void{") != std::string::npos);
    CHECK(diff.find("+fn test(): void {") != std::string::npos);

    // Identical files produce no diff
    CHECK(Formatter::generateDiff(formatted, formatted, "main.keet").empty());
}

TEST_CASE("Formatter idempotency across 20+ diverse KeetScript samples", "[fmt][idempotency]") {
    const std::vector<std::string_view> samples = {
        // 1. Minimal main
        "fn main(): int {\n    return 0;\n}\n",

        // 2. Constants and defines
        "define MAX_SIZE: int = 1024;\n"
        "const DEFAULT_NAME: string = \"keety\";\n\n"
        "fn getSize(): int {\n    return MAX_SIZE;\n}\n",

        // 3. Mut arguments and swap
        "fn swap<T>(mut x: T, mut y: T): void {\n"
        "    let temp: T = x;\n"
        "    x = y;\n"
        "    y = temp;\n"
        "}\n",

        // 4. Union types and ternary
        "fn describe(val: int | string | null): string {\n"
        "    return val != null ? \"valid\" : \"null\";\n"
        "}\n",

        // 5. If-elif-else chain
        "fn checkGrade(score: int): string {\n"
        "    if (score >= 90) {\n"
        "        return \"A\";\n"
        "    } elif (score >= 80) {\n"
        "        return \"B\";\n"
        "    } else {\n"
        "        return \"C\";\n"
        "    }\n"
        "}\n",

        // 6. Array literal
        "fn getNumbers(): array<int> {\n"
        "    return [1, 2, 3, 4, 5];\n"
        "}\n",

        // 7. Map literal
        "fn getMap(): map<string, int> {\n"
        "    return {\"first\": 1, \"second\": 2};\n"
        "}\n",

        // 8. Class with constructor/methods
        "class Counter {\n"
        "    pub let value: int = 0;\n\n"
        "    fn increment(): void {\n"
        "        this.value += 1;\n"
        "    }\n"
        "}\n",

        // 9. Enum definition
        "enum HttpMethod: int {\n"
        "    Get = 1,\n"
        "    Post = 2,\n"
        "    Put = 3,\n"
        "    Delete = 4,\n"
        "}\n",

        // 10. While loop with break
        "fn drain(): void {\n"
        "    while (hasItems()) {\n"
        "        popItem();\n"
        "    }\n"
        "}\n",

        // 11. For-in loop
        "fn sumAll(nums: array<int>): int {\n"
        "    let total: int = 0;\n"
        "    for (const n in nums) {\n"
        "        total += n;\n"
        "    }\n"
        "    return total;\n"
        "}\n",

        // 12. Switch pattern match
        "fn classify(x: int): int {\n"
        "    switch (x) {\n"
        "        case (0) {\n"
        "            return 0;\n"
        "        }\n"
        "        default {\n"
        "            return 1;\n"
        "        }\n"
        "    }\n"
        "}\n",

        // 13. Zonk with error recovery block
        "fn safeDivide(a: int, b: int): int {\n"
        "    return a / b zonk (err) {\n"
        "        yield 0;\n"
        "    };\n"
        "}\n",

        // 14. Complex binary expressions with precedence
        "fn mathPrecedence(a: int, b: int, c: int): int {\n"
        "    return (a + b) * c - a / b;\n"
        "}\n",

        // 15. Type assertion as
        "fn unbox(val: string | null): string {\n"
        "    if (val != null) {\n"
        "        return val as string;\n"
        "    }\n"
        "    return \"\";\n"
        "}\n",

        // 16. Template string with multiple expressions
        "fn formatGreeting(first: string, last: string): string {\n"
        "    return `Hello, ${first} ${last}!`;\n"
        "}\n",

        // 17. Closure expression
        "fn makeAdder(offset: int): (val: int) => int {\n"
        "    return fn(val: int): int {\n"
        "        return val + offset;\n"
        "    };\n"
        "}\n",

        // 18. Structured error type
        "errtype NetworkError {\n"
        "    pub let code: int;\n"
        "    pub let message: string;\n"
        "}\n",

        // 19. Export statements
        "export let activeUsers: int = 0;\n\n"
        "export fn getActiveUsers(): int {\n"
        "    return activeUsers;\n"
        "}\n",

        // 20. Multiple imports across tiers
        "import fs;\n"
        "import io;\n"
        "import net.http;\n\n"
        "import \"github.com/keet-pkg/uuid\";\n\n"
        "import \"../config\";\n"
        "import \"./helpers\";\n\n"
        "fn run(): void {\n"
        "    io.println(\"running\");\n"
        "}\n",

        // 21. Multi-line array literal with trailing comma
        "fn getLongList(): array<string> {\n"
        "    return [\n"
        "        \"first_long_element_value_here\",\n"
        "        \"second_long_element_value_here\",\n"
        "        \"third_long_element_value_here\",\n"
        "    ];\n"
        "}\n"
    };

    Formatter formatter;
    for (size_t i = 0; i < samples.size(); ++i) {
        std::string formattedOnce = formatter.format(samples[i]);
        CHECK(formattedOnce == samples[i]);

        std::string formattedTwice = formatter.format(formattedOnce);
        CHECK(formattedTwice == formattedOnce);
    }
}
