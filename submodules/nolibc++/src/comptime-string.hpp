#pragma once
#include "int.hpp"

namespace noxx::comptime {
template <usize N>
struct String {
    char data[N > 0 ? N : 1] = {};

    constexpr auto size() const -> usize {
        return N;
    }

    constexpr auto empty() const -> bool {
        return size() == 0;
    }

    constexpr auto operator[](const int i) const -> char {
        if(i >= 0) {
            return data[i];
        } else {
            return data[size() + i];
        }
    }

    constexpr auto operator==(String other) const -> bool {
        if(size() != other.size()) {
            return false;
        }
        for(auto i = usize(0); i < size(); i += 1) {
            if(data[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    constexpr String() {
    }

    constexpr String(const char c) {
        static_assert(N == 1);
        data[0] = c;
    }

    constexpr String(const char (&str)[N + 1]) {
        for(auto i = usize(0); i < N; i += 1) {
            data[i] = str[i];
        }
    }
};

template <usize N>
String(const char (&)[N]) -> String<N - 1>;

template <String a, String b, String... rests>
constexpr auto concat_fn() -> auto {
    if constexpr(sizeof...(rests) == 0) {
        auto ret = String<a.size() + b.size()>();
        __builtin_memcpy(ret.data, a.data, a.size());
        __builtin_memcpy(ret.data + a.size(), b.data, b.size());
        return ret;
    } else {
        return concat_fn<concat_fn<a, b>(), rests...>();
    }
}
template <String a, String b, String... rests>
constexpr auto concat = concat_fn<a, b, rests...>();

constexpr auto npos = (usize)-1;

template <String str, char key, usize index>
constexpr auto find_fn() -> usize {
    if constexpr(index < str.size() && str[index] == key) {
        return index;
    } else if constexpr(index + 1 < str.size()) {
        return find_fn<str, key, index + 1>();
    } else {
        return npos;
    }
}
template <String str, char key, usize index = 0>
constexpr auto find = find_fn<str, key, index>();

template <String str, usize index, usize len>
constexpr auto substr_fn() -> String<len> {
    auto ret = String<len>();
    __builtin_memcpy(ret.data, str.data + index, len);
    return ret;
}
template <String str, usize index, usize len = str.size() - index>
constexpr auto substr = substr_fn<str, index, len>();

// to int
constexpr auto char_table = String("0123456789abcdef");

template <char c, int base>
constexpr auto char_to_int_fn() -> int {
    constexpr auto n = find<char_table, c>;
    static_assert(n != npos && n < base);
    return n;
}
template <char c, int base>
constexpr auto char_to_int = char_to_int_fn<c, base>();

template <String str, int base, int total>
constexpr auto str_to_int_fn() -> int {
    if constexpr(str.size() == 0) {
        return total;
    } else if constexpr(str[0] == '+') {
        return +str_to_int_fn<substr<str, 1>, base, total>();
    } else if constexpr(str[0] == '-') {
        return -str_to_int_fn<substr<str, 1>, base, total>();
    } else {
        constexpr auto n = char_to_int<str[0], base>;
        return str_to_int_fn<substr<str, 1>, base, total * base + n>();
    }
}

template <String str, int base = 10>
constexpr auto str_to_int = str_to_int_fn<str, base, 0>();

// to string
template <usize num, String str = "">
constexpr auto uint_to_str_fn() -> auto {
    if constexpr(num == 0) {
        if constexpr(!str.empty()) {
            return str;
        } else {
            return "0";
        }
    } else {
        constexpr auto c = '0' + num % 10;
        return uint_to_str_fn<num / 10, concat<String<1>(c), str>>();
    }
}

template <auto num>
constexpr auto int_to_str_fn() -> auto {
    if constexpr(num >= 0) {
        return uint_to_str_fn<num>();
    } else {
        return concat<String("-"), uint_to_str_fn<-num>()>;
    }
}
template <auto num>
constexpr auto int_to_str = int_to_str_fn<num>();
} // namespace noxx::comptime
