#include <noxx/algorithm.hpp>
#include <noxx/array.hpp>

#include "hmac-sha256.hpp"
#include "kdf.hpp"

namespace crypto {
namespace {
constexpr auto digest_size = Sha256::Digest::size();
} // namespace

auto hmac_sha256_vector(const noxx::Span<const u8> key, const noxx::Span<const noxx::Span<const u8>> pieces, const Sha256::DigestMutRef mac) -> void {
    auto ctx = HmacSha256(key);
    for(auto i = usize(0); i < pieces.size(); i += 1) {
        ctx.update(pieces[i]);
    }
    ctx.finish(mac);
}

auto sha256_prf(const noxx::Span<const u8> key, const noxx::Span<const u8> label, const noxx::Span<const u8> context, const noxx::Span<u8> out) -> void {
    const auto bits    = u16(out.size() * 8);
    const auto len_le  = noxx::Array<u8, 2>{u8(bits), u8(bits >> 8)};
    auto       counter = u16(1);
    auto       pos     = usize(0);
    while(pos < out.size()) {
        const auto ctr_le = noxx::Array<u8, 2>{u8(counter), u8(counter >> 8)};
        const auto pieces = noxx::to_array<noxx::Span<const u8>>({
            ctr_le,
            label,
            context,
            len_le,
        });
        auto       hash   = Sha256::Digest();
        hmac_sha256_vector(key, pieces, hash);
        const auto take = noxx::min(out.size() - pos, digest_size);
        for(auto i = usize(0); i < take; i += 1) {
            out[pos + i] = hash[i];
        }
        pos += take;
        counter += 1;
    }
}

auto hkdf_expand(const noxx::Span<const u8> prk, const noxx::Span<const u8> info, const noxx::Span<u8> out) -> void {
    // RFC 5869: T(0) empty; T(i) = HMAC(prk, T(i-1) | info | i)
    auto prev     = Sha256::Digest();
    auto prev_len = usize(0);
    auto iter     = u8(1);
    auto pos      = usize(0);
    while(pos < out.size()) {
        const auto pieces = noxx::to_array<noxx::Span<const u8>>({
            {prev.data, prev_len},
            info,
            {&iter, 1},
        });
        auto       hash   = Sha256::Digest();
        hmac_sha256_vector(prk, pieces, hash);
        const auto take = noxx::min(out.size() - pos, digest_size);
        for(auto i = usize(0); i < take; i += 1) {
            out[pos + i] = hash[i];
        }
        pos += take;
        for(auto i = usize(0); i < digest_size; i += 1) {
            prev[i] = hash[i];
        }
        prev_len = digest_size;
        iter += 1;
    }
}
} // namespace crypto
