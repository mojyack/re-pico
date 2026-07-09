#include <noxx/bits.hpp>

#include "../hw/m33.hpp"
#include "sleep.hpp"

auto usleep(u64 us) -> void {
    SYSTICK_REGS.reload             = sys_clock / 1'000'000 - 1; // 1us per wrap
    SYSTICK_REGS.current            = 0;
    SYSTICK_REGS.control_and_status = BF(hw::systick::ControlAndStatus::Enable, 1) |
                                      BF(hw::systick::ControlAndStatus::CPUClockSource, 1);
    while(us > 0) {
        if(SYSTICK_REGS.control_and_status & hw::systick::ControlAndStatus::CountFlag) {
            us -= 1;
        }
    }
    SYSTICK_REGS.control_and_status = 0;
}
