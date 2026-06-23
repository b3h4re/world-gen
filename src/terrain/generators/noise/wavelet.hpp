#pragma once

#include "terrain/generators/generator.hpp"

namespace wgen {

    class WaveletNoise2d : public Generator {
    public:
        using FloatFunction = float (*)(float);
        // Filter parameters
        constexpr static float A = 0.25F;
        constexpr static float B = 0.5F;
        constexpr static float C = 0.25F;


        constexpr static float FREQUENCY_SCALE = 32.14231234;

        WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::uint32_t seed,
                      FloatFunction funcInterpolate = defaultPerlinInterp);
        WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight,
                      FloatFunction funcInterpolate = defaultPerlinInterp);

        void setSeed(std::uint32_t newSeed) override {
            Generator::setSeed(newSeed);
            generateRandomValues();
        }

    private:
        FloatFunction funcInterpolate_;

        std::size_t gridWidth_;
        std::size_t gridHeight_;
        HeightMap<float> randomValues;

        void generateRandomValues();
        float randomAt(std::size_t i, std::size_t j) const;
        inline constexpr static auto hFilter = &lowPassFilter<A, B, C>;
        float separableFilter(int a, int b) const;
        float smoothedGridAt(std::size_t i, std::size_t j) const;
        float waveletNoiseTile(std::size_t i, std::size_t j) const;

        float noise(std::size_t x, std::size_t y) const override;
    };

}
