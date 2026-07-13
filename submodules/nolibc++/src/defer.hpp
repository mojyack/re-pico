#if !defined(NOXX_DEFER_H)
// only once per TU
#include "utility.hpp"

namespace noxx {
template <class F>
struct Defer {
    F fn;
    Defer(F&& fn)
        : fn(noxx::forward<F>(fn)) {
    }
    ~Defer() {
        fn();
    }
};
} // namespace noxx

#define NOXX_DEFER_H
#endif

#if !defined(NOXX_DEFER_MACROS_DEFINED)
// macro definitions
#pragma push_macro("defer_concat_impl")
#undef defer_concat_impl
#pragma push_macro("defer_concat")
#undef defer_concat
#pragma push_macro("defer")
#undef defer
#define defer_concat_impl(x, y) x##y
#define defer_concat(x, y)      defer_concat_impl(x, y)
#define defer                   noxx::Defer defer_concat(defer_, __LINE__) = [&]()
#define NOXX_DEFER_MACROS_DEFINED
#else
#undef NOXX_DEFER_MACROS_DEFINED
#pragma pop_macro("defer_concat_impl")
#pragma pop_macro("defer_concat")
#pragma pop_macro("defer")
#endif
