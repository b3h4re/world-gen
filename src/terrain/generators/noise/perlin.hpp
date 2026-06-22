#pragma once

#include "terrain/generators/generator.hpp"

namespace wgen {

    float defaultPerlinInterp(float t);

    class PerlinNoise2d : public Generator {
    public:
        using FloatFunction = float (*)(float);

        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed,
                      FloatFunction funcInterpolate = defaultPerlinInterp);
        PerlinNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                      FloatFunction funcInterpolate = defaultPerlinInterp);

        void setSeed(std::uint32_t newSeed) override;
        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) override;

    private:
        void generateGradients();
        float noise(std::size_t x, std::size_t y) const;
        std::size_t sampleWidth() const;
        std::size_t sampleHeight() const;

        static float lerp(float a, float b, float c);

        FloatFunction funcInterpolate_;
        std::size_t gridWidth_;
        std::size_t gridHeight_;
        std::size_t dotsPerCell_;
        HeightMap<glm::vec2> gradients_;
    };

}
