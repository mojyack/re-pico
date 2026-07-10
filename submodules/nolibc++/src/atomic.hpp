#pragma once
#include "utility.hpp"

namespace noxx::atomic {
enum memory_order {
    relaxed,
    consume, // load-consume
    acquire, // load-acquire
    release, // store-release
    acq_rel, // store-release load-acquire
    seq_cst  // store-release load-acquire
};

template <class T, class U = T>
auto store(T& a, U&& b, const int order = memory_order::relaxed) -> void {
    __atomic_store_n(&a, noxx::forward<U>(b), order);
}

template <class T, class U = T>
auto exchange(T& a, U&& b, const int order = memory_order::relaxed) -> T {
    return __atomic_exchange_n(&a, noxx::forward<U>(b), order);
}
} // namespace noxx::atomic
