#pragma once
#include <noxx/format.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

namespace halow {
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
