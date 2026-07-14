#include "crypto/sha256.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

auto digest_matches(const char* const message, const char* const hex) -> bool {
    u8 digest[crypto::Sha256::digest_size];
    crypto::sha256((const u8*)message, strlen(message), digest);
    ensure(test::matches(digest, sizeof(digest), hex));
    return true;
}

// fips 180-4 / nist cavs vectors
auto known_vectors() -> bool {
    ensure(digest_matches("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    ensure(digest_matches("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    ensure(digest_matches("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    return true;
}

auto million_a() -> bool {
    auto ctx = crypto::Sha256();
    u8   chunk[997]; // deliberately not block-aligned to exercise buffering
    memset(chunk, 'a', sizeof(chunk));
    auto remain = usize(1000000);
    while(remain > 0) {
        const auto take = remain < sizeof(chunk) ? remain : sizeof(chunk);
        ctx.update(chunk, take);
        remain -= take;
    }
    u8 digest[crypto::Sha256::digest_size];
    ctx.finish(digest);
    ensure(test::matches(digest, sizeof(digest), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    return true;
}

auto split_update() -> bool {
    const auto message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    auto       ctx     = crypto::Sha256();
    ctx.update((const u8*)message, 13);
    ctx.update((const u8*)message + 13, strlen(message) - 13);
    u8 digest[crypto::Sha256::digest_size];
    ctx.finish(digest);
    ensure(test::matches(digest, sizeof(digest), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    ensure(known_vectors());
    ensure(million_a());
    ensure(split_update());
    printf("pass\n");
    return 0;
}
