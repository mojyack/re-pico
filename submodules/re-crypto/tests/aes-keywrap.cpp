#include "crypto/aes-keywrap.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

auto main() -> int {
    constexpr auto error_value = 1;
    // rfc 3394 section 4.1: 128-bit key data with 128-bit kek
    auto kek  = crypto::Aes128::Key();
    auto data = noxx::Array<u8, 16>();
    ensure(test::from_hex("000102030405060708090a0b0c0d0e0f", kek) == kek.size());
    ensure(test::from_hex("00112233445566778899aabbccddeeff", data) == data.size());
    const auto aes = crypto::Aes128(kek);

    auto wrapped = noxx::Array<u8, 24>();
    ensure(crypto::aes_key_wrap(aes, data, wrapped.data));
    ensure(test::matches(wrapped, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5"));

    auto unwrapped = noxx::Array<u8, 16>();
    ensure(crypto::aes_key_unwrap(aes, wrapped, unwrapped.data));
    ensure(unwrapped == data, "unwrap mismatch");

    // corrupted ciphertext must be rejected
    wrapped[3] ^= 1;
    ensure(!crypto::aes_key_unwrap(aes, wrapped, unwrapped.data));
    // bad sizes must be rejected
    ensure(!crypto::aes_key_unwrap(aes, {wrapped.data, 20}, unwrapped.data));

    printf("pass\n");
    return 0;
}
