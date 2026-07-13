#include <print>

#include "noxx/charconv.hpp"
#include "noxx/string-view.hpp"

#include "noxx/assert.hpp"

namespace {
constexpr auto error_value = false;

template <class T>
auto equals(const noxx::StringView str, const int base, const T expected) -> bool {
    unwrap(v, noxx::from_chars<T>(str, base));
    ensure(v == expected);
    return true;
}

template <class T>
auto rejects(const noxx::StringView str, const int base = 10) -> bool {
    ensure(!noxx::from_chars<T>(str, base));
    return true;
}

auto decimal() -> bool {
    ensure(equals<u32>("0", 10, 0));
    ensure(equals<u32>("42", 10, 42));
    ensure(equals<u32>("4294967295", 10, 4294967295u)); // u32 max
    ensure(equals<i32>("2147483647", 10, 2147483647));  // i32 max
    return true;
}

auto sign() -> bool {
    ensure(equals<i32>("-1", 10, -1));
    ensure(equals<i32>("-2147483648", 10, -2147483648)); // i32 min
    ensure(equals<i32>("+7", 10, 7));
    ensure(equals<u32>("+7", 10, 7));
    ensure(rejects<u32>("-7")); // unsigned rejects negative
    return true;
}

auto bases() -> bool {
    ensure(equals<u32>("ff", 16, 0xff));
    ensure(equals<u32>("FF", 16, 0xff));
    ensure(equals<u32>("deadBEEF", 16, 0xdeadbeef));
    ensure(equals<u32>("1010", 2, 0b1010));
    ensure(equals<u32>("z", 36, 35));
    ensure(equals<i16>("-2a", 16, -0x2a));
    return true;
}

auto invalid() -> bool {
    ensure(rejects<u32>(""));       // empty
    ensure(rejects<u32>("+"));      // sign only
    ensure(rejects<u32>("-"));      // sign only
    ensure(rejects<u32>("12x"));    // trailing garbage
    ensure(rejects<u32>("g", 16));  // digit out of range
    ensure(rejects<u32>("2", 2));   // digit >= base
    ensure(rejects<u32>("10", 1));  // base too small
    ensure(rejects<u32>("10", 37)); // base too large
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = -1;

    ensure(decimal());
    ensure(sign());
    ensure(bases());
    ensure(invalid());
    std::println("pass");
    return 0;
}
