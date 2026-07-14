#pragma once
#include <noxx/format.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

namespace halow {
// consumes packed structs and byte runs from a buffer; fails with null once empty
struct BufReader {
    const u8* data;
    usize     size;

    template <class T>
    auto read() -> const T* {
        return (const T*)read(sizeof(T));
    }

    auto read(const usize len) -> const u8* {
        if(size < len) {
            return nullptr;
        }
        data += len;
        size -= len;
        return data - len;
    }
};

// appends packed structs and byte runs to a fixed buffer; fails with null once full
struct BufWriter {
    noxx::Span<u8> buf;
    usize          offset = 0;

    template <class T>
    auto append() -> T* {
        return (T*)append(sizeof(T));
    }

    // reserve len uninitialized bytes
    auto append(const usize len) -> u8* {
        if(buf.size - offset < len) {
            return nullptr;
        }
        offset += len;
        return buf.data + offset - len;
    }

    auto append(const void* const src, const usize len) -> u8* {
        const auto p = append(len);
        if(p != nullptr) {
            noxx::memcpy(p, src, len);
        }
        return p;
    }

    auto written() const -> noxx::Span<const u8> {
        return {buf.data, offset};
    }
};

template <class T>
auto as_span(const T& obj) -> noxx::Span<const u8> {
    return noxx::Span<const u8>{(const u8*)&obj, sizeof(obj)};
}

template <noxx::comptime::String str, class... Args>
auto log(const Args&... args) -> void {
    auto raw = noxx::format<str>(noxx::move(args)...);
    if(raw) {
        noxx::console_out((*raw).data());
    }
}

inline auto mac_equal(const u8* const a, const u8* const b) -> bool {
    for(auto i = usize(0); i < 6; i += 1) {
        if(a[i] != b[i]) {
            return false;
        }
    }
    return true;
}
} // namespace halow
