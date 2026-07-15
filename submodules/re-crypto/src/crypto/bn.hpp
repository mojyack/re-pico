#pragma once
#include <noxx/array.hpp>
#include <noxx/span.hpp>

namespace crypto {
// 256-bit unsigned integer, little-endian 32-bit limbs (w[0] = least significant)
struct Bn256 {
    u32 w[8] = {};

    static constexpr auto from_u32(const u32 v) -> Bn256 {
        auto r = Bn256();
        r.w[0] = v;
        return r;
    }

    // 32-byte big-endian octet string (wire format)
    static constexpr auto from_be_bytes(const u8* const bytes) -> Bn256 {
        auto r = Bn256();
        for(auto i = usize(0); i < 8; i += 1) {
            const auto b = bytes + (7 - i) * 4;
            r.w[i]       = u32(b[0]) << 24 | u32(b[1]) << 16 | u32(b[2]) << 8 | u32(b[3]);
        }
        return r;
    }

    constexpr auto to_be_bytes(u8* const bytes) const -> void {
        for(auto i = usize(0); i < 8; i += 1) {
            auto b = bytes + (7 - i) * 4;
            b[0]   = u8(w[i] >> 24);
            b[1]   = u8(w[i] >> 16);
            b[2]   = u8(w[i] >> 8);
            b[3]   = u8(w[i]);
        }
    }

    constexpr auto is_zero() const -> bool {
        auto acc = u32(0);
        for(auto i = usize(0); i < 8; i += 1) {
            acc |= w[i];
        }
        return acc == 0;
    }

    constexpr auto bit(const usize i) const -> u32 {
        return (w[i / 32] >> (i % 32)) & 1;
    }

    constexpr auto operator==(const Bn256&) const -> bool = default;
};

// r = a + b, returns carry
constexpr auto bn_add(Bn256& r, const Bn256& a, const Bn256& b) -> u32 {
    auto carry = u64(0);
    for(auto i = usize(0); i < 8; i += 1) {
        const auto v = u64(a.w[i]) + b.w[i] + carry;
        r.w[i]       = u32(v);
        carry        = v >> 32;
    }
    return u32(carry);
}

// r = a - b, returns borrow
constexpr auto bn_sub(Bn256& r, const Bn256& a, const Bn256& b) -> u32 {
    auto borrow = u64(0);
    for(auto i = usize(0); i < 8; i += 1) {
        const auto v = u64(a.w[i]) - b.w[i] - borrow;
        r.w[i]       = u32(v);
        borrow       = (v >> 32) & 1;
    }
    return u32(borrow);
}

// -1 if a < b, 0 if a == b, 1 if a > b (not constant-time)
constexpr auto bn_cmp(const Bn256& a, const Bn256& b) -> int {
    for(auto i = usize(8); i > 0; i -= 1) {
        if(a.w[i - 1] != b.w[i - 1]) {
            return a.w[i - 1] < b.w[i - 1] ? -1 : 1;
        }
    }
    return 0;
}

// r = v if mask == 0xffffffff, unchanged if mask == 0
constexpr auto bn_cond_assign(Bn256& r, const Bn256& v, const u32 mask) -> void {
    for(auto i = usize(0); i < 8; i += 1) {
        r.w[i] = (r.w[i] & ~mask) | (v.w[i] & mask);
    }
}

// (a + b) mod m; requires a, b < m
constexpr auto bn_mod_add(const Bn256& a, const Bn256& b, const Bn256& m) -> Bn256 {
    auto       r      = Bn256();
    const auto carry  = bn_add(r, a, b);
    auto       s      = Bn256();
    const auto borrow = bn_sub(s, r, m);
    // subtract m if the sum overflowed 2^256 or reached m
    bn_cond_assign(r, s, u32(0) - (carry | (borrow ^ 1)));
    return r;
}

// (a - b) mod m; requires a, b < m
constexpr auto bn_mod_sub(const Bn256& a, const Bn256& b, const Bn256& m) -> Bn256 {
    auto       r      = Bn256();
    const auto borrow = bn_sub(r, a, b);
    auto       s      = Bn256();
    bn_add(s, r, m);
    bn_cond_assign(r, s, u32(0) - borrow);
    return r;
}

constexpr auto bn_rshift1(const Bn256& a) -> Bn256 {
    auto r = Bn256();
    for(auto i = usize(0); i < 8; i += 1) {
        r.w[i] = a.w[i] >> 1;
        if(i < 7) {
            r.w[i] |= a.w[i + 1] << 31;
        }
    }
    return r;
}

// r = (r << 1) | bit, returns the bit shifted out of the top
constexpr auto bn_shl1(Bn256& r, const u32 bit) -> u32 {
    const auto carry = r.w[7] >> 31;
    for(auto i = usize(7); i > 0; i -= 1) {
        r.w[i] = (r.w[i] << 1) | (r.w[i - 1] >> 31);
    }
    r.w[0] = (r.w[0] << 1) | (bit & 1);
    return carry;
}

// reduce a big-endian octet string of any length modulo m (binary long division)
constexpr auto bn_mod_bytes(const noxx::Span<const u8> be, const Bn256& m) -> Bn256 {
    auto r = Bn256();
    for(auto i = usize(0); i < be.size() * 8; i += 1) {
        const auto bit    = u32(be[i / 8] >> (7 - i % 8)) & 1;
        const auto carry  = bn_shl1(r, bit);
        auto       s      = Bn256();
        const auto borrow = bn_sub(s, r, m);
        bn_cond_assign(r, s, u32(0) - (carry | (borrow ^ 1)));
    }
    return r;
}

// Montgomery arithmetic modulo an odd 256-bit prime.
// Field element values passed to mul/sqr/exp/inv/add/sub are in the
// Montgomery domain (use to_mont/from_mont at the boundary).
struct Field {
    Bn256 m;     // modulus
    u32   n0;    // -m^-1 mod 2^32
    Bn256 rr;    // 2^512 mod m (to_mont factor)
    Bn256 one_m; // 2^256 mod m (montgomery 1)

    static constexpr auto make(const Bn256& modulus) -> Field {
        auto f = Field();
        f.m    = modulus;
        // newton iteration for m0^-1 mod 2^32 (m odd => x=m0 is correct mod 8)
        auto x = modulus.w[0];
        for(auto i = 0; i < 5; i += 1) {
            x *= 2 - modulus.w[0] * x;
        }
        f.n0 = u32(0) - x;
        // 2^256 mod m and 2^512 mod m by repeated modular doubling of 1
        auto t = Bn256::from_u32(1);
        for(auto i = 0; i < 256; i += 1) {
            t = bn_mod_add(t, t, modulus);
        }
        f.one_m = t;
        for(auto i = 0; i < 256; i += 1) {
            t = bn_mod_add(t, t, modulus);
        }
        f.rr = t;
        return f;
    }

    // montgomery multiplication (CIOS): a * b * 2^-256 mod m
    constexpr auto mul(const Bn256& a, const Bn256& b) const -> Bn256 {
        auto t = noxx::Array<u32, 10>();
        for(auto i = usize(0); i < 8; i += 1) {
            // t += a * b[i]
            auto carry = u64(0);
            for(auto j = usize(0); j < 8; j += 1) {
                const auto v = u64(a.w[j]) * b.w[i] + t[j] + carry;
                t[j]         = u32(v);
                carry        = v >> 32;
            }
            auto v = u64(t[8]) + carry;
            t[8]   = u32(v);
            t[9]   = u32(v >> 32);
            // t += m * (t[0] * n0 mod 2^32), then t >>= 32
            const auto mfac = t[0] * n0;
            carry           = (u64(mfac) * m.w[0] + t[0]) >> 32;
            for(auto j = usize(1); j < 8; j += 1) {
                const auto v2 = u64(mfac) * m.w[j] + t[j] + carry;
                t[j - 1]      = u32(v2);
                carry         = v2 >> 32;
            }
            v    = u64(t[8]) + carry;
            t[7] = u32(v);
            t[8] = t[9] + u32(v >> 32);
        }
        auto r = Bn256();
        for(auto i = usize(0); i < 8; i += 1) {
            r.w[i] = t[i];
        }
        auto       s      = Bn256();
        const auto borrow = bn_sub(s, r, m);
        bn_cond_assign(r, s, u32(0) - (t[8] | (borrow ^ 1)));
        return r;
    }

    constexpr auto sqr(const Bn256& a) const -> Bn256 {
        return mul(a, a);
    }

    constexpr auto add(const Bn256& a, const Bn256& b) const -> Bn256 {
        return bn_mod_add(a, b, m);
    }

    constexpr auto sub(const Bn256& a, const Bn256& b) const -> Bn256 {
        return bn_mod_sub(a, b, m);
    }

    constexpr auto to_mont(const Bn256& a) const -> Bn256 {
        return mul(a, rr);
    }

    constexpr auto from_mont(const Bn256& a) const -> Bn256 {
        return mul(a, Bn256::from_u32(1));
    }

    // a^e mod m; a in montgomery domain, e a plain integer.
    // square-and-always-multiply so the multiply pattern does not depend on e.
    constexpr auto exp(const Bn256& a, const Bn256& e) const -> Bn256 {
        auto r = one_m;
        for(auto i = usize(256); i > 0; i -= 1) {
            r            = sqr(r);
            const auto t = mul(r, a);
            bn_cond_assign(r, t, u32(0) - e.bit(i - 1));
        }
        return r;
    }

    // a^-1 mod m via fermat (m must be prime)
    constexpr auto inv(const Bn256& a) const -> Bn256 {
        auto e = Bn256();
        bn_sub(e, m, Bn256::from_u32(2));
        return exp(a, e);
    }
};
} // namespace crypto
