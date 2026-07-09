#include "noxx/bits.hpp"
#include "noxx/platform.hpp"

namespace {
constexpr auto udiv(uint numerator, uint denominator) -> uint {
    auto quotient = uint(0);
    for(auto i = int(noxx::msb(denominator)); i >= 0 && numerator >= denominator; i -= 1) {
        const auto u = denominator << i;
        if(numerator >= u) {
            numerator -= u;
            quotient += uint(1) << i;
        }
    }
    return quotient;
}
static_assert(udiv(8, 4) == 2);
static_assert(udiv(9, 3) == 3);
static_assert(udiv(1, 2) == 0);
static_assert(udiv(128, 2) == 64);
} // namespace

extern "C" {
auto __aeabi_uidiv(uint numerator, uint denominator) -> uint {
    return udiv(numerator, denominator);
}

auto __aeabi_idiv(int numerator, int denominator) -> int {
    const auto quotient = udiv(numerator < 0 ? -numerator : numerator, denominator < 0 ? -denominator : denominator);
    return (numerator < 0) != (denominator < 0) ? -int(quotient) : int(quotient);
}

// returns {quotient, remainder} in {r0, r1}
auto __aeabi_uidivmod(uint numerator, uint denominator) -> u64 {
    const auto quotient = udiv(numerator, denominator);
    return quotient | u64(numerator - quotient * denominator) << 32;
}

auto __aeabi_idivmod(int numerator, int denominator) -> u64 {
    const auto quotient = __aeabi_idiv(numerator, denominator);
    return u32(quotient) | u64(u32(numerator - quotient * denominator)) << 32;
}

// 64-bit unsigned long division (compiler-rt name): returns the quotient and,
// when remainder is non-null, stores the remainder. plain restoring long division.
auto __udivmoddi4(u64 numerator, u64 denominator, u64* remainder) -> u64 {
    auto quotient = u64(0);
    auto rem      = u64(0);
    for(auto i = int(63); i >= 0; i -= 1) {
        rem = (rem << 1) | ((numerator >> i) & 1);
        if(rem >= denominator) {
            rem -= denominator;
            quotient |= u64(1) << i;
        }
    }
    if(remainder != nullptr) {
        *remainder = rem;
    }
    return quotient;
}

// __aeabi_uldivmod returns {quotient, remainder} in {r0:r1, r2:r3} -- a register
// convention C++ can't express -- so marshal it around __udivmoddi4 in asm. the
// remainder pointer is the (stack-passed) third argument. Thumb-1 clean (M0/M33).
[[gnu::naked]] auto __aeabi_uldivmod(u64 /*numerator*/, u64 /*denominator*/) -> u64 {
    asm volatile(
        "push {r6, lr}\n"
        "sub  sp, sp, #16\n"
        "add  r6, sp, #8\n"    // r6 = &remainder
        "str  r6, [sp]\n"      // pass it as the 3rd arg (r0:r1, r2:r3 hold the operands)
        "bl   __udivmoddi4\n"  // quotient -> r0:r1
        "ldr  r2, [sp, #8]\n"  // remainder -> r2:r3
        "ldr  r3, [sp, #12]\n"
        "add  sp, sp, #16\n"
        "pop  {r6, pc}\n");
}

auto __aeabi_memcpy(void* dest, const void* src, usize size) -> void {
    noxx::memcpy(dest, src, size);
}
__attribute__((alias("__aeabi_memcpy"))) auto __aeabi_memcpy4(void* dest, const void* src, usize size) -> void;
__attribute__((alias("__aeabi_memcpy"))) auto __aeabi_memcpy8(void* dest, const void* src, usize size) -> void;

// optnone prevents clang from turning the loops back into the very libcalls being implemented
__attribute__((optnone)) auto __aeabi_memset(void* dest, usize size, int value) -> void {
    for(auto i = usize(0); i < size; i += 1) {
        ((u8*)dest)[i] = u8(value);
    }
}
__attribute__((alias("__aeabi_memset"))) auto __aeabi_memset4(void* dest, usize size, int value) -> void;
__attribute__((alias("__aeabi_memset"))) auto __aeabi_memset8(void* dest, usize size, int value) -> void;

auto __aeabi_memclr(void* dest, usize size) -> void {
    __aeabi_memset(dest, size, 0);
}
__attribute__((alias("__aeabi_memclr"))) auto __aeabi_memclr4(void* dest, usize size) -> void;
__attribute__((alias("__aeabi_memclr"))) auto __aeabi_memclr8(void* dest, usize size) -> void;

__attribute__((optnone)) auto strlen(const char* str) -> usize {
    auto len = usize(0);
    while(str[len] != '\0') {
        len += 1;
    }
    return len;
}
}
