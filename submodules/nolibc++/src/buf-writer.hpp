#pragma once
#include "platform.hpp"
#include "span.hpp"

#include "assert.hpp"

namespace noxx {
struct BufWriter {
    u8*   data;
    usize size;

    template <class T>
    auto append_obj(const T& obj) -> bool {
        constexpr auto error_value = false;
        unwrap(mem, alloc(sizeof(obj)));
        memcpy(&mem, &obj, sizeof(obj));
        return true;
    }

    auto append_span(Span<const u8> data) -> bool {
        constexpr auto error_value = false;
        unwrap(mem, alloc(data.size()));
        memcpy(&mem, data.data, data.size());
        return true;
    }

    template <class T>
    auto alloc_obj() -> T* {
        constexpr auto error_value = nullptr;
        unwrap(mem, alloc(sizeof(T)));
        return (T*)&mem;
    }

    auto alloc(const usize len) -> u8* {
        constexpr auto error_value = nullptr;
        ensure(size >= len);
        data += len;
        size -= len;
        return data - len;
    }

    static auto from_span(noxx::Span<u8> span) -> BufWriter {
        return {span.data, span.size()};
    }
};
} // namespace noxx
#include "assert.hpp"
