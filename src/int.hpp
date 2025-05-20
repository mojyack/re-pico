#pragma once

using u8    = unsigned char;
using u32   = unsigned int;
using usize = decltype(sizeof(void*));
static_assert(sizeof(u8) == 1);
static_assert(sizeof(u32) == 4);

using v32  = volatile u32;
using cv32 = const volatile u32;
