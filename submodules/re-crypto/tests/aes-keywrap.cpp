#include "crypto/aes-keywrap.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

auto main() -> int {
    constexpr auto error_value = 1;
    // rfc 3394 section 4.1: 128-bit key data with 128-bit kek
    u8 kek[16];
    u8 data[16];
    ensure(test::from_hex("000102030405060708090a0b0c0d0e0f", kek, sizeof(kek)) == 16);
    ensure(test::from_hex("00112233445566778899aabbccddeeff", data, sizeof(data)) == 16);
    const auto aes = crypto::Aes128(kek);

    u8 wrapped[24];
    ensure(crypto::aes_key_wrap(aes, data, sizeof(data), wrapped));
    ensure(test::matches(wrapped, sizeof(wrapped), "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5"));

    u8 unwrapped[16];
    ensure(crypto::aes_key_unwrap(aes, wrapped, sizeof(wrapped), unwrapped));
    ensure(memcmp(unwrapped, data, sizeof(data)) == 0, "unwrap mismatch");

    // corrupted ciphertext must be rejected
    wrapped[3] ^= 1;
    ensure(!crypto::aes_key_unwrap(aes, wrapped, sizeof(wrapped), unwrapped));
    // bad sizes must be rejected
    ensure(!crypto::aes_key_unwrap(aes, wrapped, 20, unwrapped));

    printf("pass\n");
    return 0;
}
