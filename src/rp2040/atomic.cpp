#include <noxx/int.hpp>

// support only single-threaded access.
// use SIO hardware spinlock for core-shared data.

namespace {
inline auto irq_save() -> u32 {
    auto primask = u32(0);
    asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) : : "memory");
    return primask;
}
inline auto irq_restore(const u32 primask) -> void {
    asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

template <class T>
auto load(const volatile void* ptr) -> T {
    const auto s = irq_save();
    const auto v = *(const volatile T*)ptr;
    irq_restore(s);
    return v;
}

template <class T>
auto store(volatile void* ptr, const T v) -> void {
    const auto s      = irq_save();
    *(volatile T*)ptr = v;
    irq_restore(s);
}

template <class T>
auto exchange(volatile void* ptr, const T v) -> T {
    const auto s   = irq_save();
    const auto p   = (volatile T*)ptr;
    const auto old = *p;
    *p             = v;
    irq_restore(s);
    return old;
}

template <class T>
auto compare_exchange(volatile void* ptr, void* expected, const T desired) -> bool {
    const auto s  = irq_save();
    const auto p  = (volatile T*)ptr;
    const auto e  = (T*)expected;
    const auto ok = *p == *e;
    if(ok) {
        *p = desired;
    } else {
        *e = *p;
    }
    irq_restore(s);
    return ok;
}
} // namespace

extern "C" {
auto __atomic_load_1(const volatile void* p, int) -> u8 { return load<u8>(p); }
auto __atomic_load_2(const volatile void* p, int) -> u16 { return load<u16>(p); }
auto __atomic_load_4(const volatile void* p, int) -> u32 { return load<u32>(p); }
auto __atomic_store_1(volatile void* p, u8 v, int) -> void { store<u8>(p, v); }
auto __atomic_store_2(volatile void* p, u16 v, int) -> void { store<u16>(p, v); }
auto __atomic_store_4(volatile void* p, u32 v, int) -> void { store<u32>(p, v); }
auto __atomic_exchange_1(volatile void* p, u8 v, int) -> u8 { return exchange<u8>(p, v); }
auto __atomic_exchange_2(volatile void* p, u16 v, int) -> u16 { return exchange<u16>(p, v); }
auto __atomic_exchange_4(volatile void* p, u32 v, int) -> u32 { return exchange<u32>(p, v); }
auto __atomic_compare_exchange_1(volatile void* p, void* e, u8 d, int, int) -> bool { return compare_exchange<u8>(p, e, d); }
auto __atomic_compare_exchange_2(volatile void* p, void* e, u16 d, int, int) -> bool { return compare_exchange<u16>(p, e, d); }
auto __atomic_compare_exchange_4(volatile void* p, void* e, u32 d, int, int) -> bool { return compare_exchange<u32>(p, e, d); }
}
