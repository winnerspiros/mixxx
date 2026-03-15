#pragma once

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "util/assert.h"

#if __has_include(<bit>)
#include <bit>
#endif

// If we don't do this then we get the C90 fabs from the global namespace which
// is only defined for double.
using std::fabs;

#define math_max std::max
#define math_min std::min
#define math_max3(a, b, c) std::max({a, b, c});
#define math_min3(a, b, c) std::min({a, b, c});

template<typename T>
    requires std::is_integral_v<T>
constexpr T roundUpToPowerOf2(T n) {
    if (n <= 0) {
        return 1;
    }
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(T) > 4) {
        n |= n >> 32;
    }
    return n + 1;
}

// Restrict value to the range [min, max]. Undefined behavior if min > max.
template<typename T>
constexpr T math_clamp(T value, T min, T max) {
    // DEBUG_ASSERT compiles out in release builds so it does not affect
    // vectorization or pipelining of clamping in tight loops.
    DEBUG_ASSERT(min <= max);
    return std::clamp(value, min, max);
}

// Check if value is between min and max.
template<typename T>
constexpr bool math_isbetween(T value, T min, T max) {
    return value >= min && value <= max;
}

#if defined(_MSC_VER) && _MSC_VER < 1928
#define CMATH_CONSTEXPR
#else
#define CMATH_CONSTEXPR constexpr
#endif

template<typename T>
    requires std::is_floating_point_v<T>
CMATH_CONSTEXPR T ratio2db(T a) {
    if (a <= 0) {
        return static_cast<T>(-1000);
    }
    return static_cast<T>(20 * log10(a));
}

template<typename T>
    requires std::is_floating_point_v<T>
CMATH_CONSTEXPR T db2ratio(T a) {
    return static_cast<T>(pow(10, a / 20));
}

#undef CMATH_CONSTEXPR

/// https://en.wikipedia.org/wiki/Sign_function
template<typename T>
    requires std::is_arithmetic_v<T>
constexpr T sgn(const T a) {
    // silence -Wtype-limits
    if constexpr (std::is_unsigned_v<T>) {
        return static_cast<T>(a > T(0));
    } else {
        return static_cast<T>(a > T(0)) - static_cast<T>(a < T(0));
    }
}
