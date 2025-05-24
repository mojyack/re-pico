#include <memory.h>
#include <stdio.h>

#include "noxx/int.hpp"

namespace noxx {
auto console_out(const char* ptr) -> bool {
    printf("%s", ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    ::memcpy(dest, src, size);
}
} // namespace noxx
