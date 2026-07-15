#include "crypto/hmac-sha256.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

auto mac_matches(const noxx::Span<const u8> key, const noxx::Span<const u8> data, const char* const hex) -> bool {
    auto mac = crypto::Sha256::Digest();
    crypto::hmac_sha256(key, data, mac);
    ensure(test::matches(mac, hex));
    return true;
}

// rfc 4231 test cases
auto case_1() -> bool {
    auto key = noxx::Array<u8, 20>();
    memset(key.data, 0x0b, key.size());
    return mac_matches(key, test::to_span("Hi There"),
                       "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
}

auto case_2() -> bool {
    return mac_matches(test::to_span("Jefe"), test::to_span("what do ya want for nothing?"),
                       "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

auto case_3() -> bool {
    auto key = noxx::Array<u8, 20>();
    memset(key.data, 0xaa, key.size());
    auto data = noxx::Array<u8, 50>();
    memset(data.data, 0xdd, data.size());
    return mac_matches(key, data,
                       "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
}

auto case_6_long_key() -> bool {
    auto key = noxx::Array<u8, 131>();
    memset(key.data, 0xaa, key.size());
    return mac_matches(key, test::to_span("Test Using Larger Than Block-Size Key - Hash Key First"),
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
