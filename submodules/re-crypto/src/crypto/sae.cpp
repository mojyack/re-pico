#include "sae.hpp"
#include "kdf.hpp"
#include "sha256.hpp"

#include <noxx/assert.hpp>

namespace crypto::sae {
namespace {
constexpr auto error_value = false;

auto span(const char* const s) -> noxx::Span<const u8> {
    auto len = usize(0);
    while(s[len] != '\0') {
        len += 1;
    }
    return {(const u8*)s, len};
}

// order - 1, computed once
auto order_minus_1() -> Bn256 {
    auto r = Bn256();
    bn_sub(r, p256::order, Bn256::from_u32(1));
    return r;
}

// pwd-seed = HKDF-Extract(salt = ssid, ikm = password) = HMAC-SHA256(ssid, password)
auto pwd_seed(const noxx::Span<const u8> ssid, const noxx::Span<const u8> password, const Sha256::DigestMutRef out) -> void {
    const auto pieces = noxx::to_array<noxx::Span<const u8>>({password});
    hmac_sha256_vector(ssid, pieces, out);
}

// u = (HKDF-Expand(seed, info, 48) mod p), then SSWU(u)
auto sswu_from_seed(const Sha256::DigestRef seed, const char* const info) -> p256::Point {
    auto pwd_value = noxx::Array<u8, 48>(); // prime_len + ceil(prime_len/2)
    hkdf_expand(seed, span(info), pwd_value);
    return p256::sswu(bn_mod_bytes(pwd_value, p256::prime));
}

// MAX(a,b) || MIN(a,b) by lexicographic compare, as required for the H2E val
auto max_min_addr(const MacAddrRef a, const MacAddrRef b, u8* const out) -> void {
    const auto cmp = noxx::memcmp(a.data, b.data, a.size());
    const auto hi  = cmp >= 0 ? a : b;
    const auto lo  = cmp >= 0 ? b : a;
    noxx::memcpy(out, hi.data, MacAddr::size());
    noxx::memcpy(out + MacAddr::size(), lo.data, MacAddr::size());
}

// a uniform scalar in [0, order) by rejection sampling
auto rand_below_order(Rng& rng, Bn256& out) -> bool {
    for(auto tries = 0; tries < 64; tries += 1) {
        auto buf = noxx::Array<u8, 32>();
        ensure(rng(buf));
        out = Bn256::from_be_bytes(buf.data);
        if(bn_cmp(out, p256::order) < 0) {
            return true;
        }
    }
    ensure(false, "rng rejection sampling exhausted");
}

// 1 < v < order
auto in_2_to_order(const Bn256& v) -> bool {
    return bn_cmp(v, Bn256::from_u32(1)) > 0 && bn_cmp(v, p256::order) < 0;
}

auto element_to_bin(const p256::Point& p, u8* const out) -> void {
    p.x.to_be_bytes(out);
    p.y.to_be_bytes(out + 32);
}
} // namespace

auto derive_pt(const noxx::Span<const u8> ssid, const noxx::Span<const u8> password) -> Pt {
    auto seed = Sha256::Digest();
    pwd_seed(ssid, password, seed);
    const auto p1 = sswu_from_seed(seed, "SAE Hash to Element u1 P1");
    const auto p2 = sswu_from_seed(seed, "SAE Hash to Element u2 P2");
    return Pt{p256::add(p1, p2)};
}

auto Session::start(const Pt& pt, const MacAddrRef own_mac, const MacAddrRef peer_mac, Rng& rng) -> bool {
    // PWE = val * PT, val = H(0^32, MAX(macs) || MIN(macs)) mod (q-1) + 1
    auto addrs = noxx::Array<u8, 2 * own_mac.size()>();
    max_min_addr(own_mac, peer_mac, addrs.data);
    auto       zero     = Sha256::Digest();
    const auto pieces   = noxx::to_array<noxx::Span<const u8>>({addrs});
    auto       val_hash = Sha256::Digest();
    hmac_sha256_vector(zero, pieces, val_hash);

    auto val = bn_mod_bytes(val_hash, order_minus_1());
    bn_add(val, val, Bn256::from_u32(1)); // 1 <= val <= q-1, no overflow
    pwe = p256::mul(pt.point, val);
    ensure(!pwe.inf && p256::is_on_curve(pwe), "degenerate PWE");

    // pick commit scalar/element: 1 < rand,mask < q with (rand+mask) mod q > 1
    for(auto tries = 0; tries < 64; tries += 1) {
        auto mask = Bn256();
        ensure(rand_below_order(rng, rand));
        ensure(rand_below_order(rng, mask));
        if(!in_2_to_order(rand) || !in_2_to_order(mask)) {
            continue;
        }
        own_scalar = p256::scalar_add(rand, mask);
        if(!in_2_to_order(own_scalar)) {
            continue;
        }
        own_element  = p256::negate(p256::mul(pwe, mask));
        committed    = true;
        have_keys    = false;
        send_confirm = 0;
        return true;
    }
    ensure(false, "could not generate commit scalar");
}

auto Session::write_commit(const noxx::Span<u8> out, const noxx::Span<const u8> token) const -> usize {
    constexpr auto error_value = usize(0);
    ensure(committed);
    // group | scalar | element, plus optional H2E anti-clogging token container
    const auto token_ie = token.size() != 0 ? 2 + 1 + token.size() : usize(0);
    const auto total    = 2 + scalar_len + element_len + token_ie;
    ensure(out.size() >= total);
    out[0] = u8(group_id);
    out[1] = u8(group_id >> 8);
    own_scalar.to_be_bytes(out.data + 2);
    element_to_bin(own_element, out.data + 2 + scalar_len);
    if(token.size() != 0) {
        auto p    = out.data + 2 + scalar_len + element_len;
        *(p += 1) = 255;                  // WLAN_EID_EXTENSION
        *(p += 1) = u8(1 + token.size()); // length
        *(p += 1) = 93;                   // WLAN_EID_EXT_ANTI_CLOGGING_TOKEN
        noxx::memcpy(p, token.data, token.size());
    }
    return total;
}

auto Session::read_commit(const noxx::Span<const u8> payload) -> bool {
    ensure(committed);
    ensure(payload.size() >= 2 + scalar_len + element_len, "short commit");
    const auto group = u16(payload.data[0]) | u16(payload.data[1]) << 8;
    ensure(group == group_id, "unexpected sae group");

    peer_scalar  = Bn256::from_be_bytes(payload.data + 2);
    peer_element = p256::Point{
        Bn256::from_be_bytes(payload.data + 2 + scalar_len),
        Bn256::from_be_bytes(payload.data + 2 + scalar_len + 32),
    };
    // validate peer contribution: scalar in [1,q-1], element a valid non-identity
    // curve point, and not a reflection of our own commit
    ensure(in_2_to_order(peer_scalar) || peer_scalar == Bn256::from_u32(1), "peer scalar out of range");
    ensure(!peer_scalar.is_zero() && bn_cmp(peer_scalar, p256::order) < 0);
    ensure(!peer_element.inf && p256::is_on_curve(peer_element), "peer element off curve");
    ensure(!(peer_scalar == own_scalar && peer_element == own_element), "sae reflection attack");

    // K = rand * (peer_scalar * PWE + peer_element); reject identity; k = K.x
    const auto k_point = p256::mul(p256::add(p256::mul(pwe, peer_scalar), peer_element), rand);
    ensure(!k_point.inf, "degenerate K");
    auto k = noxx::Array<u8, 32>();
    k_point.x.to_be_bytes(k.data);

    // keyseed = HMAC-SHA256(0^32, k)
    auto       zero    = Sha256::Digest();
    const auto kpieces = noxx::to_array<noxx::Span<const u8>>({k});
    auto       keyseed = Sha256::Digest();
    hmac_sha256_vector(zero, kpieces, keyseed);

    // context = (own_scalar + peer_scalar) mod order, 32-byte BE; PMKID = first 16
    const auto sum     = p256::scalar_add(own_scalar, peer_scalar);
    auto       context = noxx::Array<u8, 32>();
    sum.to_be_bytes(context.data);
    noxx::memcpy(pmkid.data, context.data, PMKID::size());

    // KCK || PMK = KDF-Hash-Length(keyseed, "SAE KCK and PMK", context)
    auto out = noxx::Array<u8, KCK::size() + PMK::size()>();
    sha256_prf(keyseed, span("SAE KCK and PMK"), context, out);
    noxx::memcpy(kck.data, out.data, KCK::size());
    noxx::memcpy(pmk.data, out.data + KCK::size(), PMK::size());
    have_keys = true;
    return true;
}

namespace {
// confirm = HMAC(KCK, sc | scalar1 | element1 | scalar2 | element2)
auto confirm_hash(const KCKRef kck, const u8* const sc,
                  const Bn256& scalar1, const p256::Point& element1,
                  const Bn256& scalar2, const p256::Point& element2, Sha256::DigestMutRef out) -> void {
    auto s1 = noxx::Array<u8, scalar_len>();
    auto e1 = noxx::Array<u8, element_len>();
    auto s2 = noxx::Array<u8, scalar_len>();
    auto e2 = noxx::Array<u8, element_len>();
    scalar1.to_be_bytes(s1.data);
    element_to_bin(element1, e1.data);
    scalar2.to_be_bytes(s2.data);
    element_to_bin(element2, e2.data);
    const auto pieces = noxx::to_array<noxx::Span<const u8>>({{sc, 2}, s1, e1, s2, e2});
    hmac_sha256_vector(kck, pieces, out);
}
} // namespace

auto Session::write_confirm(noxx::Span<u8> out) -> usize {
    constexpr auto error_value = usize(0);
    ensure(have_keys);
    ensure(out.size() >= 2 + Sha256::Digest::size());
    send_confirm += 1;
    out[0] = u8(send_confirm);
    out[1] = u8(send_confirm >> 8);
    confirm_hash(kck, out.data, own_scalar, own_element, peer_scalar, peer_element, {out.data + 2});
    return 2 + Sha256::Digest::size();
}

auto Session::verify_confirm(const noxx::Span<const u8> payload) const -> bool {
    ensure(have_keys);
    ensure(payload.size() >= 2 + Sha256::Digest::size(), "short confirm");
    auto verifier = Sha256::Digest();
    confirm_hash(kck, payload.data, peer_scalar, peer_element, own_scalar, own_element, verifier);
    auto diff = u8(0);
    for(auto i = usize(0); i < verifier.size(); i += 1) {
        diff |= verifier[i] ^ payload.data[2 + i];
    }
    ensure(diff == 0, "sae confirm mismatch");
    return true;
}
} // namespace crypto::sae
