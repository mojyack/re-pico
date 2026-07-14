#pragma once
#include <stdio.h>
#include <string.h>

#include <noxx/int.hpp>

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

// parses hex into buf, skipping spaces; returns bytes written or -1
inline auto from_hex(const char* str, u8* const buf, const usize limit) -> int {
    auto size = usize(0);
    while(*str != '\0') {
        if(*str == ' ') {
            str += 1;
            continue;
        }
        const auto hi = hex_value(str[0]);
        const auto lo = hex_value(str[1]);
        if(hi < 0 || lo < 0 || size >= limit) {
            return -1;
        }
        buf[size] = u8(hi << 4 | lo);
        size += 1;
        str += 2;
    }
    return int(size);
}

inline auto print_hex(const char* const label, const u8* const data, const usize size) -> void {
    printf("%s: ", label);
    for(auto i = usize(0); i < size; i += 1) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

// compares data against a hex string vector, printing both on mismatch
inline auto matches(const u8* const data, const usize size, const char* const hex) -> bool {
    u8         expected[512];
    const auto expected_size = from_hex(hex, expected, sizeof(expected));
    if(expected_size < 0 || usize(expected_size) != size || memcmp(data, expected, size) != 0) {
        print_hex("  actual  ", data, size);
        printf("  expected: %s\n", hex);
        return false;
    }
    return true;
}
} // namespace test
