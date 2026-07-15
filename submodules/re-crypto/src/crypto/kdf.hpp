#pragma once
#include "sha256.hpp"

namespace crypto {
// HMAC-SHA256 over a sequence of pieces (hostapd hmac_sha256_vector).
// this is also SAE/HKDF-Extract: prk = HMAC(salt, ikm-pieces).
auto hmac_sha256_vector(noxx::Span<const u8> key, noxx::Span<const noxx::Span<const u8>> pieces, Sha256::DigestMutRef mac) -> void;

// IEEE 802.11 KDF-Hash-Length (hostapd sha256_prf): out = T1 | T2 | ... where
// Ti = HMAC-SHA256(key, le16(i) | label | context | le16(out_bits)).
auto sha256_prf(noxx::Span<const u8> key, noxx::Span<const u8> label, noxx::Span<const u8> context, noxx::Span<u8> out) -> void;

// RFC 5869 HKDF-Expand with SHA-256 and a NUL-terminated info label
// (hostapd hmac_sha256_kdf with label = NULL, seed = info).
auto hkdf_expand(noxx::Span<const u8> prk, noxx::Span<const u8> info, noxx::Span<u8> out) -> void;
} // namespace crypto
