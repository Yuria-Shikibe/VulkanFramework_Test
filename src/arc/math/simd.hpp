#pragma once

#define SIMD_ENABLED 0

#if SIMD_ENABLED

#include <xmmintrin.h>
#include <immintrin.h>

#define SIMD_DISABLED_CONSTEXPR
#else
#define SIMD_DISABLED_CONSTEXPR constexpr
#endif