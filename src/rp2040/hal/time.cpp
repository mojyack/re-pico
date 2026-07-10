#include <hal/time.hpp>

#include "../hw/timer.hpp"
#include "time.hpp"

namespace time {
auto now() -> u64 {
    // reading time_low_read latches time_high_read, so read low first
    const auto low = TIMER_REGS.time_low_read;
    return low | u64(TIMER_REGS.time_high_read) << 32;
}

auto delay(const u64 us) -> void {
    const auto until = now() + us;
    while(now() < until) {
    }
}
} // namespace time
