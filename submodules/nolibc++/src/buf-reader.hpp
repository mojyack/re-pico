#pragma once
#include "optional.hpp"
#include "span.hpp"

namespace noxx {
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

    auto read_span(const usize len) -> noxx::Optional<noxx::Span<const u8>> {
        const auto ptr = read(len);
        if(ptr) {
            return noxx::Span<const u8>{ptr, len};
        } else {
            return noxx::nullopt;
        }
    }

    static auto from_span(noxx::Span<const u8> span) -> BufReader {
        return {span.data, span.size()};
    }
};
} // namespace noxx
