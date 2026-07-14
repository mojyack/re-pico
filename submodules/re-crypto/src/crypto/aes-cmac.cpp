#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "aes-cmac.hpp"

namespace crypto {
namespace {
constexpr auto block = Aes128::block_size;

// doubling in gf(2^128) with polynomial x^128+x^7+x^2+x+1
auto gf128_dbl(u8* const b) -> void {
    const auto msb = b[0] & 0x80;
    for(auto i = usize(0); i < block - 1; i += 1) {
        b[i] = u8(b[i] << 1 | b[i + 1] >> 7);
    }
    b[block - 1] <<= 1;
    if(msb) {
        b[block - 1] ^= 0x87;
    }
}
} // namespace

auto aes_cmac(const Aes128& aes, const u8* const data, const usize size, u8* const mac) -> void {
    // subkeys from L = aes(0)
    auto subkey = noxx::Array<u8, block>();
    aes.encrypt_block(subkey.data, subkey.data);
    gf128_dbl(subkey.data); // k1
    const auto full_last = size != 0 && size % block == 0;
    if(!full_last) {
        gf128_dbl(subkey.data); // k2
    }

    auto       x        = noxx::Array<u8, block>();
    const auto num_prev = size == 0 ? usize(0) : (size + block - 1) / block - 1;
    for(auto i = usize(0); i < num_prev; i += 1) {
        for(auto j = usize(0); j < block; j += 1) {
            x[j] ^= data[i * block + j];
        }
        aes.encrypt_block(x.data, x.data);
    }

    auto       last = noxx::Array<u8, block>();
    const auto rem  = size - num_prev * block;
    noxx::memcpy(last.data, data + num_prev * block, rem);
    if(!full_last) {
        last[rem] = 0x80;
    }
    for(auto j = usize(0); j < block; j += 1) {
        x[j] ^= last[j] ^ subkey[j];
    }
    aes.encrypt_block(x.data, mac);
}
} // namespace crypto
