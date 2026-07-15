#pragma once
#include <stdio.h>
#include <string.h>

#include <noxx/array.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

namespace test {
inline auto hex_value(const char c) -> int {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// parses hex into out, skipping spaces; returns bytes written or -1
inline auto from_hex(const char* str, noxx::Span<u8> out) -> int {
    auto size = usize(0);
    while(*str != '\0') {
        if(*str == ' ') {
            str += 1;
            continue;
        }
        const auto hi = hex_value(str[0]);
        const auto lo = hex_value(str[1]);
        if(hi < 0 || lo < 0 || size >= out.size()) {
            return -1;
        }
        out[size] = u8(hi << 4 | lo);
        size += 1;
        str += 2;
    }
    return int(size);
}

inline auto print_hex(const char* const label, const noxx::Span<const u8> data) -> void {
    printf("%s: ", label);
    for(auto i = usize(0); i < data.size(); i += 1) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

// compares data against a hex string vector, printing both on mismatch
inline auto matches(const noxx::Span<const u8> data, const char* const hex) -> bool {
    auto       expected      = noxx::Array<u8, 512>();
    const auto expected_size = from_hex(hex, expected);
    if(expected_size < 0 || usize(expected_size) != data.size() || (data != noxx::Span<u8>{expected.data, data.size()})) {
        print_hex("  actual  ", data);
        printf("  expected: %s\n", hex);
        return false;
    }
    return true;
}

inline auto to_span(const char* str) -> noxx::Span<const u8> {
    return {(const u8*)str, strlen(str)};
}
} // namespace test
