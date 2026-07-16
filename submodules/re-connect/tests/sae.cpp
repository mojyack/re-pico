#include "connect/sae.hpp"
#include "crypto/sha256.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

namespace sae = connect::sae;

// deterministic RNG: hands out scripted 32-byte scalars in order
struct ScriptRng : crypto::Rng {
    noxx::Span<const char* const> words;
    usize                         pos = 0;

    auto operator()(const noxx::Span<u8> out) -> bool override {
        ensure(pos < words.size());
        ensure(out.size() == 32);
        ensure(test::from_hex(words[pos], out) == int(out.size()));
        pos += 1;
        return true;
    }

    ScriptRng(const noxx::Span<const char* const> words)
        : words(words) {
    }
};

const auto sta_words = noxx::to_array({
    "1111111111111111111111111111111111111111111111111111111111111111",
    "2222222222222222222222222222222222222222222222222222222222222222",
});
const auto ap_words  = noxx::to_array({
    "3333333333333333333333333333333333333333333333333333333333333333",
    "4444444444444444444444444444444444444444444444444444444444444444",
});

constexpr auto sta_mac = noxx::to_array<u8>({0x0c, 0xbf, 0x74, 0x00, 0x00, 0x0a});
constexpr auto ap_mac  = noxx::to_array<u8>({0x00, 0x60, 0xad, 0x80, 0x1f, 0x51});

auto point_matches(const crypto::p256::Point& p, const char* const x_hex, const char* const y_hex) -> bool {
    auto x = noxx::Array<u8, 32>();
    auto y = noxx::Array<u8, 32>();
    p.x.to_be_bytes(x.data);
    p.y.to_be_bytes(y.data);
    ensure(test::matches(x, x_hex));
    ensure(test::matches(y, y_hex));
    return true;
}

// derive_pt / PWE reproduce the independent Python oracle
auto known_answer() -> bool {
    const auto pt = sae::derive_pt(test::to_span("halow-test"), test::to_span("password"));
    ensure(point_matches(pt.point,
                         "aa77616e22063d03bc93f702a516e16cc811ab3b02b82a5ab17a3a86c52cef7b",
                         "7b6fac950de67442a7f45d362618e359ccfc962dd86448d9e26924d07abf8e94"));

    auto srng = ScriptRng(sta_words);
    auto sta  = sae::Session();
    ensure(sta.start(pt, sta_mac, ap_mac, srng));
    ensure(point_matches(sta.pwe,
                         "5598ffb02185f03211d2d7b1eb0b2cb19f6121e364eceeb7a6035c3671afd3d8",
                         "c0df43d9b9099ac958a391a2df9c0bca6dd561ab6a83f7b21f0936f7a47a698e"));

    auto       commit = noxx::Array<u8, 128>();
    const auto clen   = sta.write_commit(commit);
    ensure(clen == 2 + 32 + 64);
    ensure(commit[0] == 19 && commit[1] == 0, "group id le16");
    ensure(test::matches({commit.data + 2, 32},
                         "3333333333333333333333333333333333333333333333333333333333333333"));
    ensure(test::matches({commit.data + 34, 64},
                         "e7f0ff23a61c2671a6c952fbb33f24a60e6b80f8b768286db15350082fa9f415"
                         "429724e2905ca5e7cddd8a077327cf98c6f24667542972b82cd2d010b3288594"));
    return true;
}

// full two-party exchange: STA and AP each run a Session, swap commit and
// confirm, and must agree on PMK/PMKID and accept each other's confirm
auto mutual_handshake() -> bool {
    const auto pt = sae::derive_pt(test::to_span("halow-test"), test::to_span("password"));

    auto srng = ScriptRng(sta_words);
    auto arng = ScriptRng(ap_words);
    auto sta  = sae::Session();
    auto ap   = sae::Session();
    ensure(sta.start(pt, sta_mac, ap_mac, srng));
    ensure(ap.start(pt, ap_mac, sta_mac, arng));

    auto       sta_commit = noxx::Array<u8, 128>();
    auto       ap_commit  = noxx::Array<u8, 128>();
    const auto sc_len     = sta.write_commit(sta_commit);
    const auto ac_len     = ap.write_commit(ap_commit);
    ensure(sc_len != 0 && ac_len != 0);

    // each side processes the peer's commit (note: peer/own MAC order swapped)
    ensure(sta.read_commit({ap_commit.data, ac_len}));
    ensure(ap.read_commit({sta_commit.data, sc_len}));

    // both derived the same PMK / PMKID and the oracle's values
    ensure(sta.pmk == ap.pmk, "pmk disagree");
    ensure(sta.pmkid == ap.pmkid, "pmkid disagree");
    ensure(test::matches(sta.kck, "f1dcc6679a8edcc14112743d12aa5f7552ad4b4c291a7173eaf10c248714b425"));
    ensure(test::matches(sta.pmk, "a45db8a0b3725a6ac7e9fedec3721d1d594998a1167443c32d8ca3edb8f45753"));
    ensure(test::matches(sta.pmkid, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));

    constexpr auto confirm_len = crypto::Sha256::Digest::size();

    auto       sta_confirm = noxx::Array<u8, 128>();
    auto       ap_confirm  = noxx::Array<u8, 128>();
    const auto scf         = sta.write_confirm(sta_confirm);
    const auto acf         = ap.write_confirm(ap_confirm);
    ensure(scf == 2 + confirm_len && acf == 2 + confirm_len);
    ensure(test::matches({sta_confirm.data + 2, confirm_len}, "08347086d301499ebafbb96d64c0b992bb353bd60db14d2b42d01718798dcf94"));
    ensure(test::matches({ap_confirm.data + 2, confirm_len}, "56e3b6d219fd1cfd575bb13172b0fcf711b4b45c8ef9e5e7c76c55b18e7191cb"));

    // and each accepts the other's confirm
    ensure(sta.verify_confirm({ap_confirm.data, acf}));
    ensure(ap.verify_confirm({sta_confirm.data, scf}));

    // a corrupted confirm is rejected
    ap_confirm[5] ^= 0x01;
    ensure(!sta.verify_confirm({ap_confirm.data, acf}));
    return true;
}

// a wrong password yields a different PMK and confirm rejection
auto wrong_password() -> bool {
    const auto good = sae::derive_pt(test::to_span("halow-test"), test::to_span("password"));
    const auto bad  = sae::derive_pt(test::to_span("halow-test"), test::to_span("wrongpass"));

    auto srng = ScriptRng(sta_words);
    auto arng = ScriptRng(ap_words);
    auto sta  = sae::Session();
    auto ap   = sae::Session();
    ensure(sta.start(good, sta_mac, ap_mac, srng));
    ensure(ap.start(bad, ap_mac, sta_mac, arng));

    auto       sc  = noxx::Array<u8, 128>();
    auto       ac  = noxx::Array<u8, 128>();
    const auto scl = sta.write_commit(sc);
    const auto acl = ap.write_commit(ac);
    ensure(sta.read_commit({ac.data, acl}));
    ensure(ap.read_commit({sc.data, scl}));

    ensure(sta.pmk != ap.pmk, "mismatched passwords must not agree");
    auto       ap_confirm = noxx::Array<u8, 128>();
    const auto acf        = ap.write_confirm(ap_confirm);
    ensure(!sta.verify_confirm({ap_confirm.data, acf}), "wrong-password confirm must fail");
    return true;
}

// reflection attack: a peer echoing our own commit back must be rejected
auto reflection_rejected() -> bool {
    const auto pt   = sae::derive_pt(test::to_span("halow-test"), test::to_span("password"));
    auto       srng = ScriptRng(sta_words);
    auto       sta  = sae::Session();
    ensure(sta.start(pt, sta_mac, ap_mac, srng));
    auto       commit = noxx::Array<u8, 128>();
    const auto clen   = sta.write_commit(commit);
    ensure(!sta.read_commit({commit.data, clen}), "own commit reflected must be rejected");
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    ensure(known_answer());
    ensure(mutual_handshake());
    ensure(wrong_password());
    ensure(reflection_rejected());
    printf("pass\n");
    return 0;
}
