#include <chrono>
#include <thread>

#include "coop/platform.hpp"

namespace coop {
namespace {
auto base = std::chrono::system_clock::now();
};

auto now_us() -> u64 {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - base).count();
}

auto sleep_until(const u64 time) -> void {
    auto now = now_us();
    while(now < time) {
        std::this_thread::sleep_for(std::chrono::microseconds(time - now));
        now = now_us();
    }
}
} // namespace coop
