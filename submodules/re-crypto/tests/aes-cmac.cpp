#include "crypto/aes-cmac.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

// rfc 4493 test vectors (all use the same key and message prefix)
constexpr auto key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
constexpr auto msg_hex = "6bc1bee22e409f96e93d7e117393172a"
                         "ae2d8a571e03ac9c9eb76fac45af8e51"
                         "30c81c46a35ce411e5fbc1191a0a52ef"
                         "f69f2445df4f9b17ad2b417be66c3710";

auto mac_matches(const crypto::Aes128& aes, const u8* const msg, const usize size, const char* const hex) -> bool {
    u8 mac[crypto::Aes128::block_size];
    crypto::aes_cmac(aes, msg, size, mac);
    ensure(test::matches(mac, sizeof(mac), hex));
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    u8 key[16];
    u8 msg[64];
    ensure(test::from_hex(key_hex, key, sizeof(key)) == 16);
    ensure(test::from_hex(msg_hex, msg, sizeof(msg)) == 64);
    const auto aes = crypto::Aes128(key);
    ensure(mac_matches(aes, msg, 0, "bb1d6929e95937287fa37d129b756746"));
    ensure(mac_matches(aes, msg, 16, "070a16b46b4d4144f79bdd9dd04a287c"));
    ensure(mac_matches(aes, msg, 40, "dfa66747de9ae63030ca32611497c827"));
    ensure(mac_matches(aes, msg, 64, "51f0bebf7e3b9d92fc49741779363cfe"));
    printf("pass\n");
    return 0;
}
