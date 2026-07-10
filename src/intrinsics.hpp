#pragma once

inline auto __enable_irq() -> void {
    asm volatile("cpsie i" ::: "memory");
}

inline auto __disable_irq() -> void {
    asm volatile("cpsid i" ::: "memory");
}

#if !__has_builtin(__wfi)
inline auto __wfi() -> void {
    asm volatile("wfi" ::: "memory");
}
#endif

inline auto __isb() -> void {
    asm volatile("isb" ::: "memory");
}
