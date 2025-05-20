#include "io-bank0.hpp"
#include "resets.hpp"
#include "sio.hpp"

extern "C" {
auto entry() -> void {
    RESETS_REGS.reset &= ~(resets::ResetNum::IO_BANK0);
    while(!(RESETS_REGS.reset_done & resets::ResetNum::IO_BANK0)) {
    }

    IOBANK0_REGS.status_control[25].control = iobank0::GPIOControlFuncSel::SIO;

    SIO_REGS.gpio_oe_set = 1 << 25;
    // SIO_REGS.gpio_out_xor = 1 << 25;

    while(true) {
        for(auto i = 0; i < 100000; i += 1) {
        }
        SIO_REGS.gpio_out_xor = 1 << 25;
    }
}
}
