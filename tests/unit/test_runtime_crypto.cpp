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

TEST_CASE("keet::crypto hash functions NIST & RFC test vectors", "[runtime][crypto]") {
    SECTION("SHA-256 test vectors") {
        REQUIRE(keet::crypto::sha256("") ==
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        REQUIRE(keet::crypto::sha256("abc") ==
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        REQUIRE(keet::crypto::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    SECTION("SHA-512 test vectors") {
        REQUIRE(keet::crypto::sha512("") ==
                "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
        REQUIRE(keet::crypto::sha512("abc") ==
                "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    }

    SECTION("SHA3-256 test vectors") {
        REQUIRE(keet::crypto::sha3_256("") ==
                "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a");
        REQUIRE(keet::crypto::sha3_256("abc") ==
                "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532");
    }

    SECTION("SHA3-512 test vectors") {
        REQUIRE(keet::crypto::sha3_512("") ==
                "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
        REQUIRE(keet::crypto::sha3_512("abc") ==
                "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
    }

    SECTION("BLAKE3 official test vectors") {
        REQUIRE(keet::crypto::blake3("") ==
                "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262");
        REQUIRE(keet::crypto::blake3("abc") ==
                "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85");

        // Multi-chunk input across multiple 1024-byte boundaries (2500 bytes)
        std::string large_payload(2500, 'A');
        std::string h1 = keet::crypto::blake3(large_payload);
        REQUIRE(h1.size() == 64);

        // Streaming matches one-shot for large payload
        auto hasher = keet::crypto::createHash("blake3");
        hasher->update(large_payload.substr(0, 500));
        hasher->update(large_payload.substr(500, 1200));
        hasher->update(large_payload.substr(1700));
        REQUIRE(hasher->digestHex() == h1);
    }
}

TEST_CASE("keet::crypto streaming Hasher engine", "[runtime][crypto]") {
    SECTION("incremental update matches one-shot across algorithms") {
        const std::vector<std::string> algos = {
            "sha256", "sha512", "sha3-256", "sha3-512", "blake3"
        };
        std::string full_text = "The quick brown fox jumps over the lazy dog";

        for (const auto& algo : algos) {
            auto h = keet::crypto::createHash(algo);
            h->update("The quick ");
            h->update("brown fox ");
            h->update("jumps over ");
            h->update("the lazy dog");
            std::string streaming_digest = h->digestHex();

            std::string one_shot;
            if (algo == "sha256") one_shot = keet::crypto::sha256(full_text);
            else if (algo == "sha512") one_shot = keet::crypto::sha512(full_text);
            else if (algo == "sha3-256") one_shot = keet::crypto::sha3_256(full_text);
            else if (algo == "sha3-512") one_shot = keet::crypto::sha3_512(full_text);
            else if (algo == "blake3") one_shot = keet::crypto::blake3(full_text);

            REQUIRE(streaming_digest == one_shot);

            // Re-use after reset
            h->reset();
            h->update(full_text);
            REQUIRE(h->digestHex() == one_shot);
        }
    }

    SECTION("unsupported algorithm throws CryptoError") {
        REQUIRE_THROWS_AS(keet::crypto::createHash("md5_insecure"), keet::error);
    }
}

TEST_CASE("keet::crypto HMAC RFC 4231 test vectors", "[runtime][crypto]") {
    SECTION("RFC 4231 Test Case 1 (20-byte key)") {
        std::string key(20, '\x0b');
        std::string data = "Hi There";

        REQUIRE(keet::crypto::hmacSha256(key, data) ==
                "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
        REQUIRE(keet::crypto::hmacSha512(key, data) ==
                "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
    }

    SECTION("RFC 4231 Test Case 2 (Jefe)") {
        std::string key = "Jefe";
        std::string data = "what do ya want for nothing?";

        REQUIRE(keet::crypto::hmacSha256(key, data) ==
                "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
        REQUIRE(keet::crypto::hmacSha512(key, data) ==
                "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");
    }

    SECTION("RFC 4231 Test Case 6 (Large key hashed first)") {
        std::string key(131, '\xaa');
        std::string data = "Test Using Larger Than Block-Size Key - Hash Key First";

        REQUIRE(keet::crypto::hmacSha256(key, data) ==
                "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
        REQUIRE(keet::crypto::hmacSha512(key, data) ==
                "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598");
    }

    SECTION("streaming HMAC matches one-shot") {
        auto hmac = keet::crypto::createHmac("sha256", "my-secret-key");
        hmac.update("streamed ");
        hmac.update("payload");
        REQUIRE(hmac.digestHex() == keet::crypto::hmacSha256("my-secret-key", "streamed payload"));
    }
}

TEST_CASE("keet::crypto EncryptedPayload serialization", "[runtime][crypto]") {
    keet::crypto::EncryptedPayload p;
    p.iv = "123456789012";
    p.tag = "abcdefghijklmnop";
    p.ciphertext = "secret message payload";
    p.aad = "auth metadata";

    std::string combined = p.toCombinedHex();
    auto deserialized = keet::crypto::EncryptedPayload::fromCombinedHex(combined, "auth metadata");

    REQUIRE(deserialized.iv == p.iv);
    REQUIRE(deserialized.tag == p.tag);
    REQUIRE(deserialized.ciphertext == p.ciphertext);
    REQUIRE(deserialized.aad == p.aad);

    REQUIRE_THROWS_AS(keet::crypto::EncryptedPayload::fromCombinedHex("invalid_no_colons"), keet::error);
    REQUIRE_THROWS_AS(keet::crypto::EncryptedPayload::fromCombinedHex("only:one_colon"), keet::error);
}

TEST_CASE("keet::crypto ChaCha20-Poly1305 RFC 8439 AEAD", "[runtime][crypto]") {
    std::string key = keet::crypto::fromHex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    std::string nonce = keet::crypto::fromHex("070000004041424344454647");
    std::string aad = keet::crypto::fromHex("50515253c0c1c2c3c4c5c6c7");
    std::string plaintext = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";

    SECTION("RFC 8439 Section 2.8.2 test vector") {
        auto enc = keet::crypto::encryptChaCha20Poly1305(key, nonce, plaintext, aad);

        std::string expected_ct = "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116";
        std::string expected_tag = "1ae10b594f09e26a7e902ecbd0600691";

        REQUIRE(keet::crypto::toHex(enc.ciphertext) == expected_ct);
        REQUIRE(keet::crypto::toHex(enc.tag) == expected_tag);

        std::string dec = keet::crypto::decryptChaCha20Poly1305(key, nonce, enc.ciphertext, enc.tag, aad);
        REQUIRE(dec == plaintext);
    }

    SECTION("tamper resistance and authentication failure") {
        auto enc = keet::crypto::encryptChaCha20Poly1305(key, nonce, plaintext, aad);

        // Tamper ciphertext
        std::string tampered_ct = enc.ciphertext;
        tampered_ct[0] ^= 0x01;
        REQUIRE_THROWS_AS(keet::crypto::decryptChaCha20Poly1305(key, nonce, tampered_ct, enc.tag, aad), keet::error);

        // Tamper tag
        std::string tampered_tag = enc.tag;
        tampered_tag[0] ^= 0x01;
        REQUIRE_THROWS_AS(keet::crypto::decryptChaCha20Poly1305(key, nonce, enc.ciphertext, tampered_tag, aad), keet::error);

        // Tamper AAD
        std::string tampered_aad = aad + "x";
        REQUIRE_THROWS_AS(keet::crypto::decryptChaCha20Poly1305(key, nonce, enc.ciphertext, enc.tag, tampered_aad), keet::error);

        // Incorrect key
        std::string wrong_key = key;
        wrong_key[0] ^= 0x01;
        REQUIRE_THROWS_AS(keet::crypto::decryptChaCha20Poly1305(wrong_key, nonce, enc.ciphertext, enc.tag, aad), keet::error);
    }

    SECTION("parameter validation") {
        REQUIRE_THROWS_AS(keet::crypto::encryptChaCha20Poly1305("short_key", nonce, plaintext), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::encryptChaCha20Poly1305(key, "short_nonce", plaintext), keet::error);
    }
}

TEST_CASE("keet::crypto AES-256-GCM NIST SP 800-38D AEAD", "[runtime][crypto]") {
    std::string key = keet::crypto::fromHex("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308");
    std::string iv = keet::crypto::fromHex("cafebabefacedbaddecaf888");
    std::string plaintext = keet::crypto::fromHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255");

    SECTION("NIST SP 800-38D Test Case 14 vector") {
        auto enc = keet::crypto::encryptAesGcm(key, iv, plaintext, "");

        std::string expected_ct = "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad";
        std::string expected_tag = "b094dac5d93471bdec1a502270e3cc6c";

        REQUIRE(keet::crypto::toHex(enc.ciphertext) == expected_ct);
        REQUIRE(keet::crypto::toHex(enc.tag) == expected_tag);

        std::string dec = keet::crypto::decryptAesGcm(key, iv, enc.ciphertext, enc.tag, "");
        REQUIRE(dec == plaintext);
    }

    SECTION("roundtrip with non-empty AAD") {
        std::string aad = "authenticated session headers";
        std::string secret = "top secret keet payload 2026";
        auto enc = keet::crypto::encryptAesGcm(key, iv, secret, aad);

        std::string dec = keet::crypto::decryptAesGcm(key, iv, enc.ciphertext, enc.tag, aad);
        REQUIRE(dec == secret);

        // Tamper ciphertext
        std::string tampered_ct = enc.ciphertext;
        tampered_ct[0] ^= 0x01;
        REQUIRE_THROWS_AS(keet::crypto::decryptAesGcm(key, iv, tampered_ct, enc.tag, aad), keet::error);

        // Tamper AAD
        REQUIRE_THROWS_AS(keet::crypto::decryptAesGcm(key, iv, enc.ciphertext, enc.tag, "wrong aad"), keet::error);

        // Tamper tag
        std::string tampered_tag = enc.tag;
        tampered_tag[0] ^= 0x01;
        REQUIRE_THROWS_AS(keet::crypto::decryptAesGcm(key, iv, enc.ciphertext, tampered_tag, aad), keet::error);
    }

    SECTION("parameter validation") {
        REQUIRE_THROWS_AS(keet::crypto::encryptAesGcm("short_key", iv, plaintext), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::encryptAesGcm(key, "short_iv", plaintext), keet::error);
    }
}

TEST_CASE("keet::crypto Code 128 barcode encoding and decoding", "[runtime][crypto]") {
    SECTION("roundtrip encoding and decoding") {
        const std::vector<std::string> test_strings = {
            "HELLO",
            "123456",
            "Keet-2026",
            "Standard Crypto Library",
            "Test_#123@xyz!"
        };

        for (const auto& text : test_strings) {
            std::string bitstream = keet::crypto::encodeCode128(text);
            REQUIRE(!bitstream.empty());

            std::string decoded = keet::crypto::decodeCode128(bitstream);
            REQUIRE(decoded == text);
        }
    }

    SECTION("SVG and ASCII visual renderers") {
        std::string text = "KEET-BARCODE";
        std::string svg = keet::crypto::encodeCode128Svg(text, 50, 2);
        REQUIRE(svg.rfind("<svg", 0) == 0);
        REQUIRE(svg.find("</svg>") != std::string::npos);
        REQUIRE(svg.find("<rect") != std::string::npos);

        std::string ascii = keet::crypto::encodeCode128Ascii(text);
        REQUIRE(!ascii.empty());
        REQUIRE(ascii.find("█") != std::string::npos);
    }

    SECTION("error handling") {
        // Empty string
        REQUIRE_THROWS_AS(keet::crypto::encodeCode128(""), keet::error);

        // Non-ASCII or control characters not supported in Code 128-B
        REQUIRE_THROWS_AS(keet::crypto::encodeCode128("Hello\tWorld"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::encodeCode128("Hello\x80World"), keet::error);

        // Bitstream decoding errors
        REQUIRE_THROWS_AS(keet::crypto::decodeCode128(""), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::decodeCode128("00000000"), keet::error);
        REQUIRE_THROWS_AS(keet::crypto::decodeCode128("1111"), keet::error);

        // Corrupted checksum
        std::string valid_bits = keet::crypto::encodeCode128("CORRUPT_ME");
        // Flip a bit in the checksum symbol (just before stop symbol of 13 bits)
        size_t checksum_bit_pos = valid_bits.rfind('1');
        if (checksum_bit_pos >= 20) {
            valid_bits[checksum_bit_pos - 15] = (valid_bits[checksum_bit_pos - 15] == '1') ? '0' : '1';
            REQUIRE_THROWS_AS(keet::crypto::decodeCode128(valid_bits), keet::error);
        }
    }
}

TEST_CASE("keet::crypto QR Code ISO/IEC 18004 generation and decoding", "[runtime][crypto]") {
    SECTION("roundtrip short text (Version 1)") {
        std::string text = "HELLO";
        auto qr = keet::crypto::generateQr(text);
        REQUIRE(qr.version == 1);
        REQUIRE(qr.size == 21);
        REQUIRE(qr.matrix.size() == 21);
        REQUIRE(qr.matrix[0].size() == 21);

        // Top-left finder pattern outer ring checks
        REQUIRE(qr.matrix[0][0] == true);
        REQUIRE(qr.matrix[0][6] == true);
        REQUIRE(qr.matrix[6][0] == true);
        REQUIRE(qr.matrix[6][6] == true);

        std::string decoded = keet::crypto::decodeQr(qr.matrix);
        REQUIRE(decoded == text);
    }

    SECTION("roundtrip medium text requiring higher version (Version 2+)") {
        std::string url = "https://keet-lang.org/docs/crypto";
        auto qr = keet::crypto::generateQr(url);
        REQUIRE(qr.version >= 2);
        REQUIRE(qr.size == 17 + 4 * qr.version);

        std::string decoded = keet::crypto::decodeQr(qr.matrix);
        REQUIRE(decoded == url);
    }

    SECTION("roundtrip complex payload") {
        std::string sentence = "The quick brown fox jumps over the lazy dog. 1234567890!";
        auto qr = keet::crypto::generateQr(sentence);

        std::string decoded = keet::crypto::decodeQr(qr.matrix);
        REQUIRE(decoded == sentence);
    }

    SECTION("renderers SVG and ASCII") {
        auto qr = keet::crypto::generateQr("keet");
        std::string svg = qr.toSvg(4);
        REQUIRE(svg.rfind("<svg", 0) == 0);
        REQUIRE(svg.find("</svg>") != std::string::npos);
        REQUIRE(svg.find("<rect") != std::string::npos);

        std::string ascii = qr.toAscii(2);
        REQUIRE(!ascii.empty());
        REQUIRE((ascii.find("█") != std::string::npos || ascii.find("▀") != std::string::npos || ascii.find("▄") != std::string::npos));
    }

    SECTION("error handling") {
        // Empty payload
        REQUIRE_THROWS_AS(keet::crypto::generateQr(""), keet::error);

        // Oversized payload (> 106 bytes for version 6-M)
        std::string huge(200, 'x');
        REQUIRE_THROWS_AS(keet::crypto::generateQr(huge), keet::error);

        // Invalid matrix dimensions in decoder
        std::vector<std::vector<keet::BoolByte>> small_matrix(10, std::vector<keet::BoolByte>(10, 0));
        REQUIRE_THROWS_AS(keet::crypto::decodeQr(small_matrix), keet::error);
    }
}



