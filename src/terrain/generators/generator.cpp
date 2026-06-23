#include "generator.hpp"


namespace wgen {

    std::size_t wrapIndex(std::size_t index, std::size_t size) {
        return index % size;
    }

    float defaultPerlinInterp(float t) {
        return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    }

    float lerp(float a, float b, float c) { return a + c * (b - a); }

}
