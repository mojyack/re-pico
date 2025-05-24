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
        return small.length & 0b0111'1111;
    } else {
        return large.length;
    }
}

auto String::data() -> char* {
    if(is_small()) {
        return small.data;
    } else {
        return large.data;
    }
}

auto String::data() const -> const char* {
    return ((String*)this)->data();
}

auto String::resize(const usize new_size) -> bool {
#define error_act return false
    if(new_size <= size()) {
        // shrink, no allocation
        if(is_small()) {
            small.length = 0b1000'0000 | new_size;
        } else {
            large.length = new_size;
        }
        data()[new_size] = '\0';
        return true;
    }
    if(is_small()) {
        if(new_size < sizeof(Small::data)) {
            // still small, no allocation
            small.length     = 0b1000'0000 | new_size;
            data()[new_size] = '\0';
            return true;
        }
    } else {
        if(new_size < large.capacity) {
            // enough capacity, no allocation
            large.length     = new_size;
            data()[new_size] = '\0';
            return true;
        }
    }
    // need allocation
    unwrap(str, create(new_size * 2)); // over allocation
    memcpy(str.data(), data(), size() + 1);
    str.large.length = new_size;
    clear();
    *this = move(str);
    return true;
#undef error_act
}

auto String::clear() -> void {
    if(!is_small()) {
        free(large.data);
    }
    small.length = 0b1000'0000;
}

auto String::append(const StringView str) -> bool {
#define error_act return false
    const auto prev_size = size();
    ensure(resize(size() + str.size()));
    memcpy(data() + prev_size, str.data(), str.size());
    return true;
#undef error_act
}

auto String::operator=(String&& other) -> String& {
    large              = other.large;
    other.small.length = 0b1000'0000;
    return *this;
}

String::String() {
    small.length = 0b1000'0000;
}

String::String(String&& other) {
    *this = move(other);
}

String::~String() {
    clear();
}

auto String::create(const usize capacity) -> Optional<String> {
#define error_act return Optional<String>();
    ensure(capacity <= ((usize)-1 >> 1));
    auto ret = String();
    if(capacity > sizeof(Small::data)) {
        ret.large.data = (char*)malloc(capacity);
        ensure(ret.large.data != nullptr);
        ret.large.length   = 0;
        ret.large.capacity = capacity;
    }
    return move(ret);
#undef error_act
}

auto String::create(StringView str) -> Optional<String> {
#define error_act return Optional<String>();
    const auto len = str.size();
    unwrap(ret, create(len + 1));
    memcpy(ret.data(), str.data(), len + 1);
    if(ret.is_small()) {
        ret.small.length = 0b1000'0000 | len;
    } else {
        ret.large.length = len;
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
