#pragma once
#include "int.hpp"

namespace noxx {
struct String;

struct StringView {
    const char* ptr;
    usize       length = 0;

    auto size() const -> usize;
    auto data() const -> const char*;
    auto clear() -> void;

    auto operator[](usize i) const -> char;
    auto operator==(StringView other) const -> bool;

    StringView(const char* str);
    StringView(const char* str, usize len);
    StringView(const String& str);
};
} // namespace noxx
