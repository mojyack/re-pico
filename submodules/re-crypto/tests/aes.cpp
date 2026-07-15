#include "crypto/aes.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

auto roundtrip(const char* const key_hex, const char* const pt_hex, const char* const ct_hex) -> bool {
    auto key = crypto::Aes128::Key();
    auto pt  = crypto::Aes128::Block();
    ensure(test::from_hex(key_hex, key) == key.size());
    ensure(test::from_hex(pt_hex, pt) == pt.size());
    const auto aes = crypto::Aes128(key);
    auto       out = crypto::Aes128::Block();
    aes.encrypt_block(pt, out);
    ensure(test::matches(out, ct_hex));
    aes.decrypt_block(out, out);
    ensure(out == pt, "decrypt mismatch");
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    // fips-197 appendix c.1
    ensure(roundtrip("000102030405060708090a0b0c0d0e0f",
                     "00112233445566778899aabbccddeeff",
                     "69c4e0d86a7b0430d8cdb78070b4c55a"));
    // nist sp 800-38a ecb-aes128
    ensure(roundtrip("2b7e151628aed2a6abf7158809cf4f3c",
                     "6bc1bee22e409f96e93d7e117393172a",
                     "3ad77bb40d7a3660a89ecaf32466ef97"));
    ensure(roundtrip("2b7e151628aed2a6abf7158809cf4f3c",
                     "ae2d8a571e03ac9c9eb76fac45af8e51",
                     "f5d3d58503b9699de785895a96fdbaaf"));
    printf("pass\n");
    return 0;
}
