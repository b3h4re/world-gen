#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    std::size_t wrapIndex(std::size_t index, std::size_t size);

    float defaultPerlinInterp(float t);

    float lerp(float a, float b, float c);

    float defaultReconstructionKernel(float t);

    // Low pass filter for {-1, 0, 1}, h[a, b, c]
    template<float a, float b, float c>
    float lowPassFilter(int x) {
        switch (x) {
            case -1:
                return a;
                break;
            case 0:
                return b;
                break;
            case 1:
                return c;
                break;
        }
        throw std::invalid_argument("Filter can only be used for x in {-1, 0, 1}");
    }

    class Generator {
    public:
        virtual ~Generator() = default;

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const {
            HeightMap<float> map{width, height};
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    map.at(x, y) = noise(x, y);
                }
            }

            return map;
        }

        virtual float noise(std::size_t x, std::size_t y) const = 0;

        virtual void setSeed(std::uint32_t newSeed) { seed_ = newSeed; }
        std::uint32_t getSeed() const { return seed_; }

    private:
        std::uint32_t seed_{};
    };

}
