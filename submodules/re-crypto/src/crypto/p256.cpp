#include "p256.hpp"

namespace crypto::p256 {
namespace {
constexpr auto fp = Field::make(prime);

// jacobian point, coordinates in the montgomery domain; z == 0 encodes infinity
struct JPoint {
    Bn256 x;
    Bn256 y;
    Bn256 z;

    auto is_inf() const -> bool {
        return z.is_zero();
    }
};

auto jcond_assign(JPoint& r, const JPoint& v, const u32 mask) -> void {
    bn_cond_assign(r.x, v.x, mask);
    bn_cond_assign(r.y, v.y, mask);
    bn_cond_assign(r.z, v.z, mask);
}

auto to_jacobian(const Point& p) -> JPoint {
    if(p.inf) {
        return JPoint();
    }
    return JPoint{fp.to_mont(p.x), fp.to_mont(p.y), fp.one_m};
}

auto to_affine(const JPoint& p) -> Point {
    if(p.is_inf()) {
        return infinity;
    }
    const auto zi  = fp.inv(p.z);
    const auto zi2 = fp.sqr(zi);
    return Point{
        .x = fp.from_mont(fp.mul(p.x, zi2)),
        .y = fp.from_mont(fp.mul(p.y, fp.mul(zi2, zi))),
    };
}

// dbl-2001-b for a = -3
auto jdbl(const JPoint& p) -> JPoint {
    const auto delta = fp.sqr(p.z);
    const auto gamma = fp.sqr(p.y);
    const auto beta  = fp.mul(p.x, gamma);
    const auto t0    = fp.mul(fp.sub(p.x, delta), fp.add(p.x, delta));
    const auto alpha = fp.add(fp.add(t0, t0), t0);
    const auto beta4 = fp.add(fp.add(beta, beta), fp.add(beta, beta));
    const auto x3    = fp.sub(fp.sqr(alpha), fp.add(beta4, beta4));
    const auto yz    = fp.add(p.y, p.z);
    const auto z3    = fp.sub(fp.sub(fp.sqr(yz), gamma), delta);
    const auto g2    = fp.sqr(gamma);
    const auto g4    = fp.add(g2, g2);
    const auto g8    = fp.add(g4, g4);
    const auto y3    = fp.sub(fp.mul(alpha, fp.sub(beta4, x3)), fp.add(g8, g8));
    return JPoint{x3, y3, z3};
}

// add-2007-bl
auto jadd(const JPoint& a, const JPoint& b) -> JPoint {
    if(a.is_inf()) {
        return b;
    }
    if(b.is_inf()) {
        return a;
    }
    const auto z1z1 = fp.sqr(a.z);
    const auto z2z2 = fp.sqr(b.z);
    const auto u1   = fp.mul(a.x, z2z2);
    const auto u2   = fp.mul(b.x, z1z1);
    const auto s1   = fp.mul(fp.mul(a.y, b.z), z2z2);
    const auto s2   = fp.mul(fp.mul(b.y, a.z), z1z1);
    const auto h    = fp.sub(u2, u1);
    const auto rr   = fp.sub(s2, s1);
    if(h.is_zero()) {
        // same x: either the same point (double) or opposites (infinity)
        return rr.is_zero() ? jdbl(a) : JPoint();
    }
    const auto h2 = fp.add(h, h);
    const auto i  = fp.sqr(h2);
    const auto j  = fp.mul(h, i);
    const auto r2 = fp.add(rr, rr);
    const auto v  = fp.mul(u1, i);
    const auto x3 = fp.sub(fp.sub(fp.sqr(r2), j), fp.add(v, v));
    const auto sj = fp.mul(s1, j);
    const auto y3 = fp.sub(fp.mul(r2, fp.sub(v, x3)), fp.add(sj, sj));
    const auto zs = fp.sub(fp.sub(fp.sqr(fp.add(a.z, b.z)), z1z1), z2z2);
    const auto z3 = fp.mul(zs, h);
    return JPoint{x3, y3, z3};
}
} // namespace

auto is_on_curve(const Point& p) -> bool {
    if(p.inf || bn_cmp(p.x, prime) >= 0 || bn_cmp(p.y, prime) >= 0) {
        return false;
    }
    const auto x   = fp.to_mont(p.x);
    const auto y   = fp.to_mont(p.y);
    const auto x3  = fp.mul(fp.sqr(x), x);
    const auto x3v = fp.sub(x3, fp.add(fp.add(x, x), x)); // x^3 - 3x
    const auto rhs = fp.add(x3v, fp.to_mont(coeff_b));
    return fp.sqr(y) == rhs;
}

auto add(const Point& a, const Point& b) -> Point {
    return to_affine(jadd(to_jacobian(a), to_jacobian(b)));
}

// double-and-add-always with constant-time accumulator select.
// note: the infinity fast paths inside jadd still leak the position of the
// scalar's top set bit; acceptable for random SAE scalars.
auto mul(const Point& p, const Bn256& scalar) -> Point {
    const auto pj = to_jacobian(p);
    auto       r  = JPoint();
    for(auto i = usize(256); i > 0; i -= 1) {
        r            = jdbl(r);
        const auto t = jadd(r, pj);
        jcond_assign(r, t, u32(0) - scalar.bit(i - 1));
    }
    return to_affine(r);
}

auto negate(const Point& p) -> Point {
    if(p.inf || p.y.is_zero()) {
        return p;
    }
    auto r = p;
    bn_sub(r.y, prime, p.y);
    return r;
}

auto is_quadratic_residue(const Bn256& a) -> bool {
    // legendre symbol: a^((p-1)/2) == 1
    auto pm1 = Bn256();
    bn_sub(pm1, prime, Bn256::from_u32(1));
    return fp.exp(fp.to_mont(a), bn_rshift1(pm1)) == fp.one_m;
}

auto sqrt(const Bn256& a) -> noxx::Optional<Bn256> {
    // p == 3 mod 4, so sqrt(a) = a^((p+1)/4) when a is a QR
    auto pp1 = Bn256();
    bn_add(pp1, prime, Bn256::from_u32(1));
    const auto e  = bn_rshift1(bn_rshift1(pp1));
    const auto am = fp.to_mont(a);
    const auto s  = fp.exp(am, e);
    if(fp.sqr(s) != am) {
        return noxx::nullopt;
    }
    return fp.from_mont(s);
}

auto scalar_add(const Bn256& a, const Bn256& b) -> Bn256 {
    return bn_mod_add(a, b, order);
}

auto scalar_valid(const Bn256& s) -> bool {
    return bn_cmp(s, Bn256::from_u32(1)) > 0 && bn_cmp(s, order) < 0;
}
} // namespace crypto::p256
