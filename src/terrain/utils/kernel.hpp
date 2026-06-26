#pragma once

#include "terrain/terrain.hpp"

namespace wgen {

    template<typename T>
    class Kernel : public HeightMap<T> {
    public:
        Kernel(std::size_t width, std::size_t height)
                : usedInKernel{width, height, true}, HeightMap<T>(width, height) {}

        Kernel(std::size_t width, std::size_t height, const T& defaultVal)
                : usedInKernel{width, height, true}, HeightMap<T>(width, height, defaultVal) {}

        Kernel(const HeightMap<bool>& shape)
                : HeightMap<T>{shape.width(), shape.height()}, usedInKernel{shape} {}

        Kernel(const HeightMap<bool>& shape, const T& defaultVal)
                : HeightMap<T>{shape.width(), shape.height(), defaultVal}, usedInKernel{shape} {}

        Kernel(const HeightMap<bool>& shape, const HeightMap<T>& values)
                : HeightMap<T>{values}, usedInKernel{shape} {}

        Kernel(const HeightMap<T>& values)
                : HeightMap<T>{values}, usedInKernel{values.width(), values.height(), true} {}

        const HeightMap<bool>& shape() const {
            return usedInKernel;
        }

        bool isUsed(std::size_t x, std::size_t y) const { return usedInKernel.at(x, y); }
        bool isUsed(glm::ivec2 pos) const { return usedInKernel.at(pos); }

    private:
        HeightMap<bool> usedInKernel;
    };

    template<typename T, typename N>
    HeightMap<T> conv(const HeightMap<T>& heightMap, const Kernel<N>& ker) {
        HeightMap<T> res{heightMap.width(), heightMap.height()};
        int deltaKerX = ker.width() / 2;
        int deltaKerY = ker.height() / 2;

        for (std::size_t y = 0; y < res.height(); ++y) {
            for (std::size_t x = 0; x < res.width(); ++x) {
                T val{0};
                // So each kernel applies assuming (x, y) corresponds with (w/2, h/2) of kernel
                for (int kerX = 0; kerX < ker.width(); ++kerX) {
                    for (int kerY = 0; kerY < ker.height(); ++kerY) {
                        if (!ker.isUsed(kerX, kerY)) { continue; }
                        if (!isInside({x, y}, {kerX - deltaKerX, kerY - deltaKerY}, res.width(), res.height())) {
                            continue;
                        }
                        glm::ivec2 pos = {
                            x + kerX - deltaKerX,
                            y + kerY - deltaKerY
                        };
                        val += heightMap.at(pos) * ker.at(kerX, kerY);
                    }
                }
                res.at(x, y) = val;
            }
        }
        return res;
    }

}
