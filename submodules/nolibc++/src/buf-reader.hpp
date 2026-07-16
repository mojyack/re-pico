#pragma once
#include "optional.hpp"
#include "span.hpp"
#include "type-traits.hpp"

namespace noxx {
template <class T>
concept BufReaderBuffer = requires(T& buf) {
    { buf.read(usize()) } -> same_as<const u8*>;
};

template <BufReaderBuffer Buf>
struct BufReader {
    Buf buf;

    template <class T>
    auto read() -> const T* {
        return (const T*)buf.read(sizeof(T));
    }

    auto read(const usize len) -> const u8* {
        return buf.read(len);
    }

    auto read_span(const usize len) -> noxx::Optional<noxx::Span<const u8>> {
        const auto ptr = read(len);
        if(ptr) {
            return noxx::Span<const u8>{ptr, len};
        } else {
            return noxx::nullopt;
        }
    }

    BufReader() = default;

    template <class... Args>
    BufReader(Args&&... args)
        : buf(forward<Args>(args)...) {
    }
};

struct BufReaderSpanBuffer {
    const u8* data;
    usize     size;

    auto read(usize len) -> const u8* {
        if(size < len) {
            return nullptr;
        }
        data += len;
        size -= len;
        return data - len;
    }

    BufReaderSpanBuffer() = default;

    BufReaderSpanBuffer(noxx::Span<const u8> span)
        : data(span.data),
          size(span.size()) {
    }
};

using SpanReader = BufReader<BufReaderSpanBuffer>;
} // namespace noxx
