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
    auto find(StringView key, int pos = 0) const -> int;
    auto substr(usize offset, int count = -1) const -> StringView;

    auto operator[](usize i) const -> char;
    auto operator==(StringView other) const -> bool;

    StringView() = default;
    StringView(const char* str);
    StringView(const char* str, usize len);
    StringView(const String& str);
};
} // namespace noxx
