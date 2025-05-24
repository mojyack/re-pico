#include "rom.hpp"

namespace rom {
namespace {
#define GROUP(...) __VA_ARGS__

#define LOOKUP(name, params, names)                     \
    auto lookup_##name(params)->auto {                  \
        name = (decltype(name))lookup_func(code::name); \
        return name(names);                             \
    }

LOOKUP(popcount32, u32 a1, a1);
LOOKUP(reverse32, u32 a1, a1);
LOOKUP(clz32, u32 a1, a1);
LOOKUP(ctz32, u32 a1, a1);
LOOKUP(memset, GROUP(u8* a1, u8 a2, u32 a3), GROUP(a1, a2, a3));
LOOKUP(memset4, GROUP(u8* a1, u8 a2, u32 a3), GROUP(a1, a2, a3));
LOOKUP(memcpy, GROUP(u8* a1, u8* a2, u32 a3), GROUP(a1, a2, a3));
LOOKUP(memcpy44, GROUP(u8* a1, u8* a2, u32 a3), GROUP(a1, a2, a3));
} // namespace

#define CACHE(name) auto name = &lookup_##name
CACHE(popcount32);
CACHE(reverse32);
CACHE(clz32);
CACHE(ctz32);
CACHE(memset);
CACHE(memset4);
CACHE(memcpy);
CACHE(memcpy44);
} // namespace rom
