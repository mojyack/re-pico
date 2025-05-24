#pragma once

namespace noxx {
template<class T>
auto move(T& v) -> T&& {
    return (T&&)v;
}
}
