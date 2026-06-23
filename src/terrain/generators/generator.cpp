#include "generator.hpp"

#include <cmath>

namespace wgen {

    std::size_t wrapIndex(std::size_t index, std::size_t size) {
        return index % size;
    }

    float defaultPerlinInterp(float t) {
        return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    }

    float lerp(float a, float b, float c) { return a + c * (b - a); }

    float defaultReconstructionKernel(float t) {
        if (std::abs(t) < 1) {
            return 1 - std::abs(t);
        }
        return 0;
    }

}
