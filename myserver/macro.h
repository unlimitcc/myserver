#ifndef __CC_MACRO_H__
#define __CC_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"
#include "log.h"

//指定编译器可能的优化方向
#if defined __GNUC__ || __llvm__
#   define CC_LIKELY(x)    __builtin_expect(!!(x), 1)
#   define CC_UNLIKELY(x)  __builtin_expect(!!(x), 0)

#else
#   define CC_LIKELY(x)     (x)
#   define CC_UNLIKELY(x)   (x)

#endif

#define CC_ASSERT(x) \
    if(CC_UNLIKELY(!(x))) { \
        CC_LOG_ERROR(CC_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << cc::BackTraceToString(100, 2, "    "); \
        assert(x); \
    }

#define CC_ASSERT2(x,w) \
    if(CC_UNLIKELY(!(x))) { \
        CC_LOG_ERROR(CC_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n MESSAGE: " << w \
            << "\nbacktrace:\n" \
            << cc::BackTraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif