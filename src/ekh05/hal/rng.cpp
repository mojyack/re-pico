#include <hal/rng.hpp>
#include <noxx/bits.hpp>

#include "../hw/rcc.hpp"
#include "../hw/rng.hpp"

namespace rng {
namespace {

// discard one word and reseed after a seed error (RM0456 §33.3.7)
auto recover_seed_error() -> void {
    RNG_REGS.status = ~u32(hw::rng::Status::SeedErrInt | hw::rng::Status::ClkErrInt);
    RNG_REGS.control &= ~hw::rng::Control::RngEn;
    RNG_REGS.control |= hw::rng::Control::RngEn;
}
} // namespace

auto init() -> void {
    // HSI48 is the RNG kernel clock (RNGSEL = 0, the reset default in CCIPR2)
    RCC_REGS.control |= hw::rcc::Control::HSI48On;
    while(!(RCC_REGS.control & hw::rcc::Control::HSI48Ready)) {
    }
    RCC_REGS.ahb2_enable1 |= hw::rcc::AHB2Enable1::RNG;
    RNG_REGS.control = hw::rng::Control::RngEn;
}

auto fill(const noxx::Span<u8> out) -> bool {
    auto produced = usize(0);
    while(produced < out.size()) {
        // spin until a fresh 32-bit sample is ready, handling rare faults
        auto guard = u32(0);
        while(!(RNG_REGS.status & hw::rng::Status::DataReady)) {
            if(RNG_REGS.status & (hw::rng::Status::SeedErr | hw::rng::Status::ClockErr)) {
                recover_seed_error();
            }
            guard += 1;
            if(guard > 1'000'000) {
                return false;
            }
        }
        const auto word = RNG_REGS.data;
        for(auto i = usize(0); i < 4 && produced < out.size(); i += 1) {
            out[produced] = u8(word >> (i * 8));
            produced += 1;
        }
    }
    return true;
}
} // namespace rng
