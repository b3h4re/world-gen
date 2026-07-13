#pragma once

#include "terrain/planet/cube_sphere.hpp"
#include "terrain/utils/generator_utils.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace wgen {

struct PerlinNoiseComputeSpec3D {
    float cellSize{1.0f};
    float coordinateScale{1.0F};
    std::uint32_t faceResolution{100};
    std::uint32_t padding{};
    SeedType seed{};
};
static_assert(offsetof(PerlinNoiseComputeSpec3D, cellSize) == 0);
static_assert(offsetof(PerlinNoiseComputeSpec3D, coordinateScale) == 4);
static_assert(offsetof(PerlinNoiseComputeSpec3D, faceResolution) == 8);
static_assert(offsetof(PerlinNoiseComputeSpec3D, padding) == 12);
static_assert(offsetof(PerlinNoiseComputeSpec3D, seed) == 16);
static_assert(sizeof(PerlinNoiseComputeSpec3D) == 24);


class Generator3d {
public:
    virtual ~Generator3d() = default;

    virtual CubeSphere<float> generateCubeSphere(std::size_t resolution) const {
        CubeSphere<float> cubeSphere{resolution, 0.0F};
        for (const CubeSphereFace face : FACES) {
            for (std::size_t y = 0; y < resolution; ++y) {
                for (std::size_t x = 0; x < resolution; ++x) {
                    cubeSphere.at(face, x, y) = noise(cubeSphere.pointUnitDir(face, x, y));
                }
            }
        }

        return cubeSphere;
    }

    virtual GeneratorCapabilities capabilities() const { return {}; }
    virtual std::string compShader() const {
        throw std::runtime_error("3D generator does not have a .comp shader");
    }
    virtual std::size_t specSize() const {
        throw std::runtime_error("3D generator does not provide a spec");
    }

    virtual void setSeed(SeedType newSeed) { seed_ = newSeed; }
    SeedType getSeed() const { return seed_; }
    virtual float noise(glm::vec3 point) const = 0;

private:
    SeedType seed_{};
};

} // namespace wgen
