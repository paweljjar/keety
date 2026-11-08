# KeetScript Standard Library: `crypto` Module

The `crypto` module provides a comprehensive, production-grade cryptography library built directly into the `keety` compiler and runtime. It includes constant-time data encodings, operating system entropy-backed CSPRNG, industry-standard cryptographic hashes, HMAC message authentication, authenticated symmetric ciphers (AEAD), and symbology encoders/decoders (Code 128 barcodes and ISO/IEC 18004 QR codes).

Zero external third-party C++ libraries are required; the runtime is entirely self-contained and header-only (`<keet/crypto.hpp>`), generating portable standard C++17.

---

## 1. Importing the Module

The `crypto` module can be imported using any of the four standard import forms (as well as module aliases):

```keet
// 1. Unquoted module import
import crypto;

// 2. Quoted module import
import "crypto";

// 3. Unquoted selective import
from crypto import sha256, createHash, randomUuid;

// 4. Quoted selective import
from "crypto" import sha256, createHash, randomUuid, encryptAesGcm, decryptAesGcm;
```

```keet
// Module alias import
import crypto as c;

fn main(): void {
    let hash: string = crypto.sha256("Hello, world!");
    let token: string = c.randomHex(16);
    let uuid: string = randomUuid();
}
```

---

## 2. API Reference

### 2.1 Data Encodings (Hex & Base64)

Constant-time, RFC-compliant data transformation utilities.

#### `crypto.toHex(data: string): string`
Encodes binary data or text into lowercase hexadecimal characters.

```keet
let hex: string = crypto.toHex("Hello"); // "48656c6c6f"
```

#### `crypto.fromHex(hexStr: string): string`
Decodes a lowercase or uppercase hexadecimal string into raw binary or ASCII string.
- Validates input format; odd lengths or non-hex characters trigger a zonk-able `CryptoError`.

```keet
let text: string = crypto.fromHex("48656c6c6f"); // "Hello"
let safe: string = crypto.fromHex("invalid_hex") zonk "fallback";
```

#### `crypto.toBase64(data: string): string`
Encodes data into standard RFC 4648 Base64 format with `=` padding.

```keet
let b64: string = crypto.toBase64("KeetScript"); // "S2VldFNjcmlwdA=="
```

#### `crypto.fromBase64(b64Str: string): string`
Decodes an RFC 4648 Base64 string back into binary or text.
- Invalid base64 sequences trigger a zonk-able `CryptoError`.

```keet
let text: string = crypto.fromBase64("S2VldFNjcmlwdA=="); // "KeetScript"
```

#### `crypto.toBase64Url(data: string): string`
Encodes data into URL-safe and filename-safe Base64 representation (`+` $\to$ `-`, `/` $\to$ `_`) without padding characters. Ideal for JSON Web Tokens (JWT) and URL query parameters.

```keet
let urlSafe: string = crypto.toBase64Url("KeetScript?Crypto&Safe");
```

#### `crypto.fromBase64Url(b64UrlStr: string): string`
Decodes a URL-safe Base64 string into original data.

---

### 2.2 Cryptographically Secure Pseudo-Random Number Generation (CSPRNG)

Backed directly by OS-level cryptographic entropy (`arc4random_buf` / `getentropy` on macOS/BSD, `/dev/urandom` / `getrandom` on Linux, `BCryptGenRandom` on Windows).

#### `crypto.randomBytes(size: int): string`
Generates a string of cryptographically secure random bytes of the specified length.

```keet
let key: string = crypto.randomBytes(32); // 256-bit symmetric key
```

#### `crypto.randomHex(size: int): string`
Generates a random hex string representing `size` random bytes (resulting string length is $2 \times \text{size}$).

```keet
let nonceHex: string = crypto.randomHex(16); // 32-character hex string
```

#### `crypto.randomInt(min: int, max: int): int`
Generates a uniform, unbiased cryptographically secure random 64-bit integer within the inclusive range `[min, max]`. Uses rejection sampling to eliminate modulo bias.

```keet
let dice: int = crypto.randomInt(1, 6);
```

#### `crypto.randomUuid(): string`
Generates an RFC 4122 Version 4 random UUID (format: `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`).

```keet
let id: string = crypto.randomUuid(); // "c4378aa0-1e8a-4550-9175-09350e179c11"
```

#### `crypto.timingSafeEqual(a: string, b: string): bool`
Performs a constant-time comparison of two byte buffers to eliminate timing side-channel attacks when verifying authentication tokens, signatures, or passwords.

```keet
let valid: bool = crypto.timingSafeEqual(userToken, expectedToken);
```

---

### 2.3 Cryptographic Hashes & Streaming API

High-performance, pure C++17 hash implementations.

#### `crypto.sha256(data: string): string`
Computes NIST FIPS 180-4 SHA-256 hash returned as a 64-character lowercase hexadecimal string.

```keet
let digest: string = crypto.sha256("input data");
```

#### `crypto.sha512(data: string): string`
Computes NIST FIPS 180-4 SHA-512 hash returned as a 128-character lowercase hexadecimal string.

#### `crypto.sha3_256(data: string): string`
Computes NIST FIPS 202 Keccak-based SHA3-256 hash returned as a 64-character hex string.

#### `crypto.sha3_512(data: string): string`
Computes NIST FIPS 202 Keccak-based SHA3-512 hash returned as a 128-character hex string.

#### `crypto.blake3(data: string): string`
Computes BLAKE3 cryptographic hash returned as a 64-character hex string.

#### `crypto.createHash(algorithm: string): crypto.Hasher`
Initializes an incremental streaming hash accumulator.
- Supported algorithms: `"sha256"`, `"sha512"`, `"sha3-256"`, `"sha3-512"`, `"blake3"`.

```keet
let hasher: crypto.Hasher = crypto.createHash("sha256");
hasher.update("chunk-1");
hasher.update("chunk-2");
let hexDigest: string = hasher.digestHex();
```

---

### 2.4 Message Authentication Codes (HMAC)

RFC 2104 / FIPS 198 keyed-hash message authentication.

#### `crypto.hmacSha256(key: string, data: string): string`
Computes HMAC-SHA256 message authentication code returned as a 64-character hex string.

```keet
let signature: string = crypto.hmacSha256("api-secret", "payload");
```

#### `crypto.hmacSha512(key: string, data: string): string`
Computes HMAC-SHA512 message authentication code returned as a 128-character hex string.

#### `crypto.createHmac(algorithm: string, key: string): crypto.Hmac`
Creates an incremental streaming HMAC instance.
- Supported algorithms: `"sha256"`, `"sha512"`.

```keet
let hmac: crypto.Hmac = crypto.createHmac("sha256", "my-secret-key");
hmac.update("streamed ");
hmac.update("payload");
let tag: string = hmac.digestHex();
```

---

### 2.5 Authenticated Symmetric Encryption (AEAD)

Modern authenticated encryption with associated data (AEAD) ensuring both confidentiality and integrity.

#### `class crypto.EncryptedPayload`
Data model returned by AEAD encryption functions:
- `ciphertext: string` (encrypted ciphertext bytes)
- `tag: string` (16-byte authentication tag)
- `iv: string` (12-byte initialization vector / nonce)
- `aad: string` (optional associated authenticated data)

#### `crypto.encryptChaCha20Poly1305(key: string, nonce: string, plaintext: string, aad?: string): crypto.EncryptedPayload`
Encrypts plaintext using RFC 8439 ChaCha20 stream cipher with Poly1305 one-time authenticator.
- `key`: exactly 32 bytes (256 bits).
- `nonce`: exactly 12 bytes (96 bits).
- `aad`: optional unencrypted header authenticated by the Poly1305 tag.

#### `crypto.decryptChaCha20Poly1305(key: string, nonce: string, ciphertext: string, tag: string, aad?: string): string`
Decrypts and authenticates ChaCha20-Poly1305 payload. If the ciphertext, nonce, tag, or AAD has been modified or tampered with, throws a zonk-able `CryptoError`.

```keet
let chaPayload = crypto.encryptChaCha20Poly1305(key, nonce, "Secret", "header_id");
let decrypted: string = crypto.decryptChaCha20Poly1305(
    key, nonce, chaPayload.ciphertext, chaPayload.tag, "header_id"
) zonk "Tampering Detected";
```

#### `crypto.encryptAesGcm(key: string, iv: string, plaintext: string, aad?: string): crypto.EncryptedPayload`
Encrypts plaintext using NIST SP 800-38D AES-256 in Galois/Counter Mode (GCM).
- `key`: exactly 32 bytes (256-bit AES key).
- `iv`: 12-byte initialization vector.

#### `crypto.decryptAesGcm(key: string, iv: string, ciphertext: string, tag: string, aad?: string): string`
Decrypts and verifies AES-256-GCM ciphertext and authentication tag. Throws a zonk-able `CryptoError` upon authentication tag mismatch.

```keet
let aesPayload = crypto.encryptAesGcm(key, iv, "Secret Data", "metadata");
let plain: string = crypto.decryptAesGcm(
    key, iv, aesPayload.ciphertext, aesPayload.tag, "metadata"
) zonk "Tampered";
```

---

### 2.6 Symbology: 1D Barcodes & 2D QR Codes

High-precision barcode and matrix code generators conforming to international standards.

#### `class crypto.QrCode`
Model returned by `crypto.generateQr`:
- `size: int` (module grid dimension, e.g. 21 for Version 1, 29 for Version 3)
- `version: int` (QR version: 1 to 6)
- `matrix: array<array<bool>>` (2D module grid; `true` = dark, `false` = light)
- `toSvg(margin?: int): string` (renders crisp standalone SVG XML string)
- `toAscii(margin?: int): string` (renders high-contrast Unicode half-block characters formatted with ANSI escape codes for direct scanning from terminal displays)

#### `crypto.generateQr(text: string, ecLevel?: string): crypto.QrCode`
Generates an ISO/IEC 18004 compliant QR Code using Byte mode and Medium error correction (recovers up to 15% missing data).
- Automatically evaluates all 8 data masks and selects the mask with the lowest penalty score (N1–N4).
- Adheres to standard finder, alignment, and timing patterns, ensuring instant recognition by mobile phone camera scanners (iOS and Android).

```keet
let qr: crypto.QrCode = crypto.generateQr("https://keetscript.dev", "M");

// Render to terminal display:
io.println(qr.toAscii(2));

// Export vector SVG:
fs.writeFile("qrcode.svg", qr.toSvg(4));
```

#### `crypto.decodeQr(matrix: array<array<bool>>): string`
Software QR decoder reading a 2D boolean module matrix directly into text payload.
- Reads BCH format information, de-masks data modules, unpacks Byte mode bitstream.

```keet
let originalText: string = crypto.decodeQr(qr.matrix) zonk "decode failed";
```

#### `crypto.encodeCode128(text: string): string`
Encodes ASCII text into a Code 128 (Subset B) binary pattern string (`1` for bar, `0` for space) with modulo 103 checksum and quiet zones.

#### `crypto.encodeCode128Svg(text: string, barHeight?: int, moduleWidth?: int): string`
Generates a standalone vector SVG barcode image.

#### `crypto.encodeCode128Ascii(text: string): string`
Generates a visual ASCII block representation of the Code 128 barcode suitable for console logs.

#### `crypto.decodeCode128(pattern: string): string`
Decodes a Code 128 binary pattern string (`1` and `0`) back into text, verifying the checksum.

---

## 3. Complete Example

```keet
import crypto;
import io;

fn main(): void {
    io.println("=== KeetScript Security Demo ===");

    // 1. Password hashing with salt
    let salt: string = crypto.randomHex(16);
    let hasher = crypto.createHash("sha256");
    hasher.update(salt);
    hasher.update("my_secure_password");
    let passwordHash: string = hasher.digestHex();
    io.println("Salt: " + salt);
    io.println("Password Hash: " + passwordHash);

    // 2. Authenticated encryption
    let key: string   = crypto.randomBytes(32);
    let iv: string    = crypto.randomBytes(12);
    let secret: string = "API_KEY_SECRET_98765";

    let payload = crypto.encryptAesGcm(key, iv, secret, "user:42");
    io.println("Ciphertext: " + crypto.toHex(payload.ciphertext));

    let decrypted: string = crypto.decryptAesGcm(
        key, iv, payload.ciphertext, payload.tag, "user:42"
    ) zonk "DECRYPTION_FAILED";
    io.println("Decrypted: " + decrypted);

    // 3. QR Code generation
    let qr = crypto.generateQr("https://google.com");
    io.println("Scan QR code below:");
    io.println(qr.toAscii(2));
}
```
