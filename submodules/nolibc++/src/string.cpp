#include "string.hpp"
#include "assert.hpp"
#include "malloc.hpp"
#include "platform.hpp"

namespace noxx {
auto strlen(const char* const str) -> usize {
    auto ptr = str;
    while(*ptr != '\0') {
        ptr += 1;
    }
    return ptr - str;
}

auto String::is_small() const -> bool {
    return small.length & 0b1000'0000;
}

auto String::size() const -> usize {
    if(is_small()) {
        return small.length & 0b0111'0000;
    } else {
        return large.length;
    }
}

auto String::data() const -> const char* {
    if(is_small()) {
        return small.data;
    } else {
        return large.data;
    }
}

auto String::clear() -> void {
    if(!is_small()) {
        free(large.data);
    }
    small.length = 0b1000'0000;
}

String::String() {
    small.length = 0b1000'0000;
}

String::~String() {
    clear();
}

auto String::create(const char* const str) -> Optional<String> {
#define error_act return Optional<String>();
    const auto len = strlen(str);
    ensure(len <= ((usize)-1 >> 1));
    auto ret = String();
    if(can_be_small(len)) {
        memcpy(ret.small.data, str, len + 1);
        ret.small.length = 0b1000'0000 | len;
    } else {
        ret.large.data = (char*)malloc(len + 1);
        ensure(ret.large.data != nullptr);
        memcpy(ret.large.data, str, len + 1);
        ret.large.length   = len;
        ret.large.capacity = len + 1;
    }
    return move(ret);
#undef error_act
}

auto StringView::size() const -> usize {
    return length;
}

auto StringView::data() const -> const char* {
    return ptr;
}

auto StringView::clear() -> void {
    length = 0;
}

StringView::StringView(const char* const str) {
    ptr    = str;
    length = strlen(str);
}

StringView::StringView(const String& str) {
    ptr    = str.data();
    length = str.size();
}
} // namespace noxx
