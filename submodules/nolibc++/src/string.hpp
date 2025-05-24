#pragma once
#include "int.hpp"
#include "optional.hpp"
#include "type-traits.hpp"

namespace noxx {
auto strlen(const char* str) -> usize;

// {64,32,16} -> u{64,32,16}
template <usize size>
using uintn = Conditional<size == 64, u64, Conditional<size == 32, u32, Conditional<size == 16, u16, void>>>;

// for 32bit system:
// layout:
// address    | 00 01 02 03 04 05 06 07
// when large | [data     ] [len] [cap]
// when small | [data[0]...data[14]] [l]
// last two bytes:
// bits       | 0000 0000 0000 0000
// when large | cap[7:0]  cap[15:8]
// when small | data      size
//                        ^ mode bit here

struct StringView;

struct String {
    using Size = uintn<sizeof(void*) * 8 / 2>;

    struct Large {
        char* data;
        Size  length;
        Size  capacity;
    };
    struct Small {
        char data[sizeof(Large) - 1];
        u8   length; // mapped to first byte of Large::capacity
    };
    static_assert(sizeof(Large) == sizeof(Small));

    union {
        Large large;
        Small small;
    };

    auto is_small() const -> bool;
    auto size() const -> usize;
    auto data() -> char*;
    auto data() const -> const char*;
    auto resize(usize new_size) -> bool;
    auto clear() -> void;
    auto append(StringView str) -> bool;

    auto operator=(String&& other) -> String&;

    String();
    String(String&& other);
    ~String();

    static auto create(StringView str) -> Optional<String>;
    static auto create(usize capacity) -> Optional<String>;
};

struct StringView {
    const char* ptr;
    usize       length = 0;

    auto size() const -> usize;
    auto data() const -> const char*;
    auto clear() -> void;

    StringView(const char* str);
    StringView(const String& str);
};
} // namespace noxx
