#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "aes-keywrap.hpp"

#include <noxx/assert.hpp>

namespace crypto {
namespace {
constexpr auto error_value = false;

constexpr u8 initial_value[8] = {0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6};

auto xor_counter(u8* const a, const u64 t) -> void {
    for(auto i = usize(0); i < 8; i += 1) {
        a[7 - i] ^= u8(t >> (i * 8));
    }
}
} // namespace

auto aes_key_wrap(const Aes128& kek, const u8* const data, const usize data_size, u8* const wrapped) -> bool {
    ensure(data_size % 8 == 0 && data_size >= 16);
    const auto n = data_size / 8;
    const auto a = wrapped;
    const auto r = wrapped + 8;
    noxx::memcpy(a, initial_value, 8);
    noxx::memcpy(r, data, data_size);
    auto b = noxx::Array<u8, 16>();
    for(auto j = usize(0); j < 6; j += 1) {
        for(auto i = usize(0); i < n; i += 1) {
            noxx::memcpy(b.data, a, 8);
            noxx::memcpy(b.data + 8, r + i * 8, 8);
            kek.encrypt_block(b.data, b.data);
            noxx::memcpy(a, b.data, 8);
            xor_counter(a, u64(n * j + i + 1));
            noxx::memcpy(r + i * 8, b.data + 8, 8);
        }
    }
    return true;
}

auto aes_key_unwrap(const Aes128& kek, const u8* const wrapped, const usize wrapped_size, u8* const data) -> bool {
    ensure(wrapped_size % 8 == 0 && wrapped_size >= 24);
    const auto n = wrapped_size / 8 - 1;
    auto       a = noxx::Array<u8, 8>();
    noxx::memcpy(a.data, wrapped, 8);
    noxx::memcpy(data, wrapped + 8, n * 8);
    auto b = noxx::Array<u8, 16>();
    for(auto j = usize(6); j > 0; j -= 1) {
        for(auto i = n; i > 0; i -= 1) {
            xor_counter(a.data, u64(n * (j - 1) + i));
            noxx::memcpy(b.data, a.data, 8);
            noxx::memcpy(b.data + 8, data + (i - 1) * 8, 8);
            kek.decrypt_block(b.data, b.data);
            noxx::memcpy(a.data, b.data, 8);
            noxx::memcpy(data + (i - 1) * 8, b.data + 8, 8);
        }
    }
    auto diff = u8(0);
    for(auto i = usize(0); i < 8; i += 1) {
        diff |= a[i] ^ initial_value[i];
    }
    ensure(diff == 0, "keywrap icv mismatch");
    return true;
}
} // namespace crypto
