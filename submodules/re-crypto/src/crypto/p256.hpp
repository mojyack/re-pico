#pragma once
#include <noxx/optional.hpp>

#include "bn.hpp"

namespace crypto::p256 {
// NIST P-256 (secp256r1), the SAE group 19 curve: y^2 = x^3 - 3x + b mod p
constexpr auto prime   = Bn256{{0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0xffffffff}};
constexpr auto order   = Bn256{{0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, 0xffffffff, 0xffffffff, 0x00000000, 0xffffffff}};
constexpr auto coeff_b = Bn256{{0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0, 0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8}};
constexpr auto gen_x   = Bn256{{0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81, 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2}};
constexpr auto gen_y   = Bn256{{0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357, 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2}};

// affine point in the normal (non-montgomery) domain
struct Point {
    Bn256 x;
    Bn256 y;
    bool  inf = false;

    constexpr auto operator==(const Point& o) const -> bool {
        return inf ? o.inf : (!o.inf && x == o.x && y == o.y);
    }
};

constexpr auto generator = Point{gen_x, gen_y};
constexpr auto infinity  = Point{{}, {}, true};

auto is_on_curve(const Point& p) -> bool;
auto add(const Point& a, const Point& b) -> Point;
auto mul(const Point& p, const Bn256& scalar) -> Point;
auto negate(const Point& p) -> Point;

// field element helpers for SAE hunting-and-pecking (values < p)
auto is_quadratic_residue(const Bn256& a) -> bool;
auto sqrt(const Bn256& a) -> noxx::Optional<Bn256>; // nullopt if a is not a QR

// scalar (mod group order) helpers
auto scalar_add(const Bn256& a, const Bn256& b) -> Bn256; // (a + b) mod order
auto scalar_valid(const Bn256& s) -> bool;                // 1 < s < order (SAE range)
} // namespace crypto::p256
