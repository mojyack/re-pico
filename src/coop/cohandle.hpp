#pragma once
#include "coroutine.hpp"

namespace coop {
struct Runner;

template <class T>
concept CoHandleLike = requires(T handle) {
    handle.promise().runner;
    handle.resume();
};
} // namespace coop
