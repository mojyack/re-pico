#include "platform.hpp"

#if !defined(NOXX_ASSERT_H)

#define NOXX_ASSERT_H
#pragma push_macro("STRINGIFY")
#pragma push_macro("TOSTRING")
#pragma push_macro("ensure")
#pragma push_macro("unwrap")

#undef STRINGIFY
#define STRINGIFY(n) #n
#undef TOSTRING
#define TOSTRING(n) STRINGIFY(n)

#undef ensure
#define ensure(cond)                               \
    if(!(cond)) {                                  \
        noxx::console_out("assertion failed at "); \
        noxx::console_out(__func__);               \
        noxx::console_out(":");                    \
        noxx::console_out(TOSTRING(__LINE__));     \
        noxx::console_out("\r\n");                 \
        error_act;                                 \
    }
#undef unwrap
#define unwrap(var, exp)  \
    auto var##_o = (exp); \
    ensure(var##_o, act); \
    auto& var = *var##_o;

#else

#undef NOXX_ASSERT_H
#pragma pop_macro("STRINGIFY")
#pragma pop_macro("TOSTRING")
#pragma pop_macro("ensure")
#pragma pop_macro("unwrap")
#endif
