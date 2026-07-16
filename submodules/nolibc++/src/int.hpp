#pragma once

using i8  = char;
using i16 = short;
using i32 = int;
using i64 = long long;
static_assert(sizeof(i8) == 1);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(i64) == 8);

using u8    = unsigned char;
using u16   = unsigned short;
using u32   = unsigned int;
using u64   = unsigned long long;
using usize = decltype(sizeof(void*));
static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);

using uu8  = __attribute__((aligned(1)))  u8;
using uu16 = __attribute__((aligned(1))) u16;
using uu32 = __attribute__((aligned(1))) u32;
using uu64 = __attribute__((aligned(1))) u64;

using uint = unsigned int;

using v32  = volatile u32;
using cv32 = const volatile u32;
