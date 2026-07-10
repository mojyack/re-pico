#include "platform.hpp"

#if !defined(NOXX_ASSERT_H)

#define NOXX_ASSERT_H
#pragma push_macro("STRINGIFY")
#pragma push_macro("TOSTRING")
#pragma push_macro("generic_ensure")
#pragma push_macro("generic_unwrap")
#pragma push_macro("error_act")
#pragma push_macro("ensure")
#pragma push_macro("unwrap")
#pragma push_macro("co_error_act")
#pragma push_macro("co_ensure")
#pragma push_macro("co_unwrap")

#undef STRINGIFY
#define STRINGIFY(n) #n

#undef TOSTRING
#define TOSTRING(n) STRINGIFY(n)

#pragma push_macro("generic_ensure")
#undef generic_ensure
#define generic_ensure(act, cond, ...)                   \
    if(!(cond)) {                                        \
        noxx::console_out("assertion failed at ");       \
        noxx::console_out(__FILE__);                     \
        noxx::console_out(":");                          \
        noxx::console_out(TOSTRING(__LINE__));           \
        __VA_OPT__(noxx::console_out(": " __VA_ARGS__);) \
        noxx::console_out("\r\n");                       \
        act;                                             \
    }

#pragma push_macro("generic_unwrap")
#undef generic_unwrap
#define generic_unwrap(ensure, var, exp, ...)  \
    auto var##_o = (exp);                      \
    ensure(var##_o __VA_OPT__(, __VA_ARGS__)); \
    auto& var = *var##_o;

#undef error_act
#define error_act return error_value
#undef ensure
#define ensure(cond, ...) generic_ensure(error_act, cond __VA_OPT__(, __VA_ARGS__))
#undef unwrap
#define unwrap(var, exp, ...) generic_unwrap(ensure, var, exp __VA_OPT__(, __VA_ARGS__))

#undef co_error_act
#define co_error_act co_return error_value
#undef co_ensure
#define co_ensure(cond, ...) generic_ensure(co_error_act, cond __VA_OPT__(, __VA_ARGS__))
#undef co_unwrap
#define co_unwrap(var, exp, ...) generic_unwrap(co_ensure, var, exp __VA_OPT__(, __VA_ARGS__))

#else

#undef NOXX_ASSERT_H
#pragma pop_macro("STRINGIFY")
#pragma pop_macro("TOSTRING")
#pragma pop_macro("generic_ensure")
#pragma pop_macro("generic_unwrap")
#pragma pop_macro("error_act")
#pragma pop_macro("ensure")
#pragma pop_macro("unwrap")
#pragma pop_macro("co_error_act")
#pragma pop_macro("co_ensure")
#pragma pop_macro("co_unwrap")

#endif
