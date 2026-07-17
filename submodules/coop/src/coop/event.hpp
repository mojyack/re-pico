#pragma once
#include <noxx/int.hpp>

namespace coop {
enum class EventResult : u8 {
    Ok,
    Error,
    TimedOut,
};

using TimePoint = u64;
using Duration  = u64;

constexpr auto time_infinite = Duration(-1);
} // namespace coop
