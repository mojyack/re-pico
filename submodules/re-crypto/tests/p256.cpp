#include "crypto/p256.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

namespace p256 = crypto::p256;
using crypto::Bn256;

auto bn(const char* const hex) -> Bn256 {
    auto bytes = noxx::Array<u8, 32>();
    if(test::from_hex(hex, bytes) != bytes.size()) {
        printf("bad test vector: %s\n", hex);
        return Bn256();
    }
    return Bn256::from_be_bytes(bytes.data);
}

auto bn_matches(const Bn256& v, const char* const hex) -> bool {
    auto bytes = noxx::Array<u8, 32>();
    v.to_be_bytes(bytes.data);
    ensure(test::matches(bytes, hex));
    return true;
}

auto bn_basics() -> bool {
    const auto a = bn("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296");
    ensure(a == p256::gen_x);
    ensure(bn_matches(a, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"));
    // montgomery roundtrip and multiplication identity
    constexpr auto fp = crypto::Field::make(p256::prime);
    const auto     am = fp.to_mont(a);
    ensure(fp.from_mont(am) == a);
    ensure(fp.mul(am, fp.one_m) == am);
    // (x * x^-1) == 1
    ensure(fp.mul(am, fp.inv(am)) == fp.one_m);
    return true;
}

auto curve_basics() -> bool {
    ensure(p256::is_on_curve(p256::generator));
    auto off = p256::generator;
    off.y.w[0] ^= 1;
    ensure(!p256::is_on_curve(off));

    // 2G, known nist vector
    const auto g2 = p256::add(p256::generator, p256::generator);
    ensure(p256::is_on_curve(g2));
    ensure(bn_matches(g2.x, "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978"));
    ensure(bn_matches(g2.y, "07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1"));
    ensure(p256::mul(p256::generator, Bn256::from_u32(2)) == g2);
    ensure(p256::mul(p256::generator, Bn256::from_u32(1)) == p256::generator);

    // G + (-G) == infinity; n * G == infinity
    ensure(p256::add(p256::generator, p256::negate(p256::generator)).inf);
    ensure(p256::mul(p256::generator, p256::order).inf);
    return true;
}

// rfc 5903 section 8.1 (256-bit random ecp group dh vectors)
auto ecdh_rfc5903() -> bool {
    const auto i_priv = bn("c88f01f510d9ac3f70a292daa2316de544e9aab8afe84049c62a9c57862d1433");
    const auto r_priv = bn("c6ef9c5d78ae012a011164acb397ce2088685d8f06bf9be0b283ab46476bee53");

    const auto i_pub = p256::mul(p256::generator, i_priv);
    ensure(bn_matches(i_pub.x, "dad0b65394221cf9b051e1feca5787d098dfe637fc90b9ef945d0c3772581180"));
    ensure(bn_matches(i_pub.y, "5271a0461cdb8252d61f1c456fa3e59ab1f45b33accf5f58389e0577b8990bb3"));

    const auto r_pub = p256::mul(p256::generator, r_priv);
    ensure(bn_matches(r_pub.x, "d12dfb5289c8d4f81208b70270398c342296970a0bccb74c736fc7554494bf63"));
    ensure(bn_matches(r_pub.y, "56fbf3ca366cc23e8157854c13c58d6aac23f046ada30f8353e74f33039872ab"));

    // both sides derive the same shared secret
    const auto s1 = p256::mul(r_pub, i_priv);
    const auto s2 = p256::mul(i_pub, r_priv);
    ensure(bn_matches(s1.x, "d6840f6b42f6edafd13116e0e12565202fef8e9ece7dce03812464d04b9442de"));
    ensure(s1 == s2);
    return true;
}

auto field_helpers() -> bool {
    // gy^2 is a qr and its roots are +-gy
    constexpr auto fp = crypto::Field::make(p256::prime);
    const auto     y2 = fp.from_mont(fp.sqr(fp.to_mont(p256::gen_y)));
    ensure(p256::is_quadratic_residue(y2));
    unwrap(root, p256::sqrt(y2));
    auto neg = Bn256();
    crypto::bn_sub(neg, p256::prime, root);
    ensure(root == p256::gen_y || neg == p256::gen_y);

    // p == 3 mod 4, so -1 is a non-residue
    auto minus1 = Bn256();
    crypto::bn_sub(minus1, p256::prime, Bn256::from_u32(1));
    ensure(!p256::is_quadratic_residue(minus1));
    ensure(!p256::sqrt(minus1));
    return true;
}

auto scalar_helpers() -> bool {
    // (order - 1) + 2 == 1 mod order
    auto nm1 = Bn256();
    crypto::bn_sub(nm1, p256::order, Bn256::from_u32(1));
    ensure(p256::scalar_add(nm1, Bn256::from_u32(2)) == Bn256::from_u32(1));
    ensure(p256::scalar_valid(Bn256::from_u32(2)));
    ensure(p256::scalar_valid(nm1));
    ensure(!p256::scalar_valid(Bn256::from_u32(0)));
    ensure(!p256::scalar_valid(Bn256::from_u32(1)));
    ensure(!p256::scalar_valid(p256::order));
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    ensure(bn_basics());
    ensure(curve_basics());
    ensure(ecdh_rfc5903());
    ensure(field_helpers());
    ensure(scalar_helpers());
    printf("pass\n");
    return 0;
}
