#include <iostream>
#include <cassert>
#include <cmath>

// Mocking some stuff to test math.h in isolation if needed,
// but let's try to include it directly first.
#define Q_DECL_CONSTEXPR constexpr
#define Q_DECL_RELAXED_CONSTEXPR constexpr

#include "src/util/math.h"

int main() {
    // Test math_max3
    assert(math_max3(1, 2, 3) == 3);
    assert(math_max3(5, 2, 3) == 5);
    assert(math_max3(1, 10, 3) == 10);

    // Test math_min3
    assert(math_min3(1, 2, 3) == 1);
    assert(math_min3(5, 2, 3) == 2);
    assert(math_min3(5, 10, 3) == 3);

    // Test roundUpToPowerOf2
    assert(roundUpToPowerOf2(3) == 4);
    assert(roundUpToPowerOf2(4) == 4);
    assert(roundUpToPowerOf2(5) == 8);
    assert(roundUpToPowerOf2(0) == 0);
    assert(roundUpToPowerOf2(1) == 1);

    // Test roundToFraction
    assert(std::abs(roundToFraction(1.234, 10.0) - 1.2) < 0.0001);
    assert(std::abs(roundToFraction(1.256, 10.0) - 1.3) < 0.0001);

    std::cout << "Math tests passed!" << std::endl;
    return 0;
}
