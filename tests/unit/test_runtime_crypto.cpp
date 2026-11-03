#include <catch2/catch_test_macros.hpp>
#include <keet/crypto.hpp>
#include <keet/error.hpp>

#include <set>
#include <string>

TEST_CASE("keet::crypto hex encoding and decoding", "[runtime][crypto]") {
    SECTION("roundtrip empty and basic strings") {
        REQUIRE(keet::crypto::toHex("") == "");
        REQUIRE(keet::crypto::fromHex("") == "");

        std::string text = "hello world";
        std::string hex = keet::crypto::toHex(text);
        REQUIRE(hex == "68656c6c6f20776f726c64");
        REQUIRE(keet::crypto::fromHex(hex) == text);
    }

    SECTION("supports uppercase hex during decoding") {
        REQUIRE(keet::crypto::fromHex("48454C4C4F") == "HELLO");
        REQUIRE(keet::crypto::fromHex("48454c4c6f") == "HELLo");
    }

    SECTION("roundtrip arbitrary binary payload including null bytes") {
        std::string binary;
        for (int i = 0; i < 256; ++i) {
            binary.push_back(static_cast<char>(i));
        }
        std::string hex = keet::crypto::toHex(binary);
        REQUIRE(hex.size() == 512);
        REQUIRE(keet::crypto::fromHex(hex) == binary);
    }

    SECTION("rejects odd-length hex strings") {
        REQUIRE_THROWS_AS(keet::crypto::fromHex("abc"), keet::error);
        try {
            (void)keet::crypto::fromHex("12345");
            FAIL("expected CryptoError");
        } catch (const keet::error& err) {
            REQUIRE(err.name == "CryptoError");
        }
    }

    SECTION("rejects invalid characters") {
        REQUIRE_THROWS_AS(keet::crypto::fromHex("0g"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::fromHex("zz"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::fromHex("12 34"), keet::error);
    }
}

TEST_CASE("keet::crypto Base64 and Base64URL encoding/decoding", "[runtime][crypto]") {
    SECTION("RFC 4648 standard Base64 test vectors") {
        REQUIRE(keet::crypto::toBase64("") == "");
        REQUIRE(keet::crypto::toBase64("f") == "Zg==");
        REQUIRE(keet::crypto::toBase64("fo") == "Zm8=");
        REQUIRE(keet::crypto::toBase64("foo") == "Zm9v");
        REQUIRE(keet::crypto::toBase64("foob") == "Zm9vYg==");
        REQUIRE(keet::crypto::toBase64("fooba") == "Zm9vYmE=");
        REQUIRE(keet::crypto::toBase64("foobar") == "Zm9vYmFy");

        REQUIRE(keet::crypto::fromBase64("") == "");
        REQUIRE(keet::crypto::fromBase64("Zg==") == "f");
        REQUIRE(keet::crypto::fromBase64("Zm8=") == "fo");
        REQUIRE(keet::crypto::fromBase64("Zm9v") == "foo");
        REQUIRE(keet::crypto::fromBase64("Zm9vYg==") == "foob");
        REQUIRE(keet::crypto::fromBase64("Zm9vYmE=") == "fooba");
        REQUIRE(keet::crypto::fromBase64("Zm9vYmFy") == "foobar");
    }

    SECTION("RFC 4648 Base64URL test vectors") {
        // \xfb\xff\xbf in standard Base64 is "+/+/"; in URL-safe Base64 is "-_-_"
        std::string special = "\xfb\xff\xbf";
        REQUIRE(keet::crypto::toBase64(special) == "+/+/");
        REQUIRE(keet::crypto::toBase64Url(special) == "-_-_");

        REQUIRE(keet::crypto::fromBase64Url("-_-_") == special);
        REQUIRE(keet::crypto::fromBase64Url(keet::crypto::toBase64Url("hello world!")) == "hello world!");
    }

    SECTION("rejects invalid Base64 input") {
        REQUIRE_THROWS_AS(keet::crypto::fromBase64("==="), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::fromBase64("Z"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::fromBase64("Zm9vY"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::fromBase64("Zm9!"), keet::error);
    }
}

TEST_CASE("keet::crypto CSPRNG generators", "[runtime][crypto]") {
    SECTION("randomBytes") {
        REQUIRE(keet::crypto::randomBytes(0) == "");
        REQUIRE_THROWS_AS(keet::crypto::randomBytes(-1), keet::error);

        std::string b1 = keet::crypto::randomBytes(32);
        std::string b2 = keet::crypto::randomBytes(32);
        REQUIRE(b1.size() == 32);
        REQUIRE(b2.size() == 32);
        REQUIRE(b1 != b2);
    }

    SECTION("randomHex") {
        REQUIRE(keet::crypto::randomHex(0) == "");
        REQUIRE_THROWS_AS(keet::crypto::randomHex(-5), keet::error);

        std::string hex = keet::crypto::randomHex(16);
        REQUIRE(hex.size() == 32);
        for (char c : hex) {
            REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
        }
    }

    SECTION("randomInt") {
        REQUIRE(keet::crypto::randomInt(42, 42) == 42);
        REQUIRE_THROWS_AS(keet::crypto::randomInt(100, 50), keet::error);

        bool saw_different = false;
        int64_t first = keet::crypto::randomInt(1, 1000);
        for (int i = 0; i < 50; ++i) {
            int64_t val = keet::crypto::randomInt(1, 1000);
            REQUIRE(val >= 1);
            REQUIRE(val <= 1000);
            if (val != first) saw_different = true;
        }
        REQUIRE(saw_different);

        // Negative interval
        for (int i = 0; i < 20; ++i) {
            int64_t val = keet::crypto::randomInt(-100, -50);
            REQUIRE(val >= -100);
            REQUIRE(val <= -50);
        }
    }

    SECTION("randomUuid RFC 4122 v4 compliance") {
        std::set<std::string> uuids;
        for (int i = 0; i < 25; ++i) {
            std::string uuid = keet::crypto::randomUuid();
            REQUIRE(uuid.size() == 36);
            REQUIRE(uuid[8] == '-');
            REQUIRE(uuid[13] == '-');
            REQUIRE(uuid[18] == '-');
            REQUIRE(uuid[23] == '-');
            // Version 4 check
            REQUIRE(uuid[14] == '4');
            // Variant check (8, 9, a, or b)
            char var = uuid[19];
            REQUIRE((var == '8' || var == '9' || var == 'a' || var == 'b'));
            uuids.insert(uuid);
        }
        REQUIRE(uuids.size() == 25);
    }
}

TEST_CASE("keet::crypto timingSafeEqual", "[runtime][crypto]") {
    REQUIRE(keet::crypto::timingSafeEqual("", "") == true);
    REQUIRE(keet::crypto::timingSafeEqual("hello", "hello") == true);
    REQUIRE(keet::crypto::timingSafeEqual("secret_key_12345", "secret_key_12345") == true);

    REQUIRE(keet::crypto::timingSafeEqual("hello", "world") == false);
    REQUIRE(keet::crypto::timingSafeEqual("hello", "hell") == false);
    REQUIRE(keet::crypto::timingSafeEqual("hell", "hello") == false);
    REQUIRE(keet::crypto::timingSafeEqual("a", "b") == false);
}
