#pragma once

#include "terrain/generators/generator.hpp"

#include <vector>

namespace wgen {

float defaultPerlinInterp(float t);

class PerlinNoise2d : public Generator {
public:

    PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed, float (*funcInterpolate)(float) = defaultPerlinInterp);
    PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, float (*funcInterpolate)(float) = defaultPerlinInterp);

    HeightMap<float> generateheightMap(std::size_t width, std::size_t height);
private:
    using FloatFunction = float (*)(float);
    FloatFunction funcInterpolate;

    std::size_t gridOffsetX{0};
    std::size_t gridOffsetY{0};

    std::size_t gridWidth;
    std::size_t gridHeight;
    std::size_t dotsPerCell;
    HeightMap<glm::vec2> grid;
    HeightMap<HeightMap<glm::vec3>> cells;

    float noise(std::size_t x, std::size_t y);

    static float lerp(float a, float b, float c);
};

}
