#pragma once
/* Minimal Desktop - basic integer types */
typedef unsigned char       u8;
typedef signed char         s8;
typedef unsigned short      u16;
typedef signed short        s16;
typedef unsigned int        u32;
typedef signed int          s32;
typedef unsigned long long  u64;
typedef signed long long    s64;
typedef __SIZE_TYPE__       size_t;
typedef u32                 uintptr_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define true 1
#define false 0
typedef int bool;

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))
#define NORETURN __attribute__((noreturn))
#define UNUSED(x) (void)(x)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
