#include "string-view.hpp"
#include "string.hpp"

namespace noxx {
namespace {
auto strlen(const char* const str) -> usize {
    auto ptr = str;
    while(*ptr != '\0') {
        ptr += 1;
    }
    return ptr - str;
}
} // namespace

auto StringView::size() const -> usize {
    return length;
}

auto StringView::data() const -> const char* {
    return ptr;
}

auto StringView::clear() -> void {
    length = 0;
}

auto StringView::find(const StringView key, const int pos) const -> int {
    if(key.size() > size()) {
        return -1;
    }
    const auto last = size() - key.size();
    for(auto i = usize(pos); i <= last; i += 1) {
        auto found = true;
        for(auto j = usize(0); j < key.size(); j += 1) {
            if((*this)[i + j] != key[j]) {
                found = false;
                break;
            }
        }
        if(found) {
            return int(i);
        }
    }
    return -1;
}

auto StringView::substr(const usize offset, const int count) const -> StringView {
    return StringView(ptr + offset, count >= 0 ? count : length - offset);
}

auto StringView::operator[](usize i) const -> char {
    return data()[i];
}

auto StringView::operator==(StringView other) const -> bool {
    if(size() != other.size()) {
        return false;
    }
    for(auto i = usize(0); i < size(); i += 1) {
        if((*this)[i] != other[i]) {
            return false;
        }
    }
    return true;
}

StringView::StringView(const char* const str) {
    ptr    = str;
    length = strlen(str);
}

StringView::StringView(const char* const str, const usize len) {
    ptr    = str;
    length = len;
}

StringView::StringView(const String& str) {
    ptr    = str.data();
    length = str.size();
}
} // namespace noxx
