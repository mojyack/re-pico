#include "crypto/hmac-sha256.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

auto mac_matches(const u8* const key, const usize key_size, const u8* const data, const usize size, const char* const hex) -> bool {
    u8 mac[crypto::Sha256::digest_size];
    crypto::hmac_sha256(key, key_size, data, size, mac);
    ensure(test::matches(mac, sizeof(mac), hex));
    return true;
}

// rfc 4231 test cases
auto case_1() -> bool {
    u8 key[20];
    memset(key, 0x0b, sizeof(key));
    const auto data = "Hi There";
    return mac_matches(key, sizeof(key), (const u8*)data, strlen(data),
                       "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

auto case_2() -> bool {
    const auto key  = "Jefe";
    const auto data = "what do ya want for nothing?";
    return mac_matches((const u8*)key, strlen(key), (const u8*)data, strlen(data),
                       "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

auto case_3() -> bool {
    u8 key[20];
    memset(key, 0xaa, sizeof(key));
    u8 data[50];
    memset(data, 0xdd, sizeof(data));
    return mac_matches(key, sizeof(key), data, sizeof(data),
                       "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
}

auto case_6_long_key() -> bool {
    u8 key[131];
    memset(key, 0xaa, sizeof(key));
    const auto data = "Test Using Larger Than Block-Size Key - Hash Key First";
    return mac_matches(key, sizeof(key), (const u8*)data, strlen(data),
                       "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    ensure(case_1());
    ensure(case_2());
    ensure(case_3());
    ensure(case_6_long_key());
    printf("pass\n");
    return 0;
}
