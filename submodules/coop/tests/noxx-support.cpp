#include <memory.h>
#include <stdio.h>

#include <hal/time.hpp>

#include "noxx/int.hpp"

namespace noxx {
auto console_out(const char* ptr) -> bool {
    printf("%s", ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    ::memcpy(dest, src, size);
}

auto memset(void* a, u8 c, usize size) -> void {
    ::memset(a, c, size);
}

auto memcmp(const void* a, const void* b, usize size) -> int {
    return ::memcmp(a, b, size);
}
} // namespace noxx

namespace time {
auto counter = u64(0);

auto now() -> u64 {
    return counter += 1;
}

auto delay(const u64 us) -> void {
    const auto until = now() + us;
    while(now() < until) {
    }
}
} // namespace time
