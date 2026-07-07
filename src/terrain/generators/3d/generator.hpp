#pragma once

#include "terrain/planet.hpp"
#include "terrain/utils/generator_utils.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace wgen {

struct PerlinNoiseComputeSpec3D {
    float cellSize{1.0f};
    float coordinateScale{1.0F};
    float planetRadius{100.0f};
    alignas(8) std::uint32_t dotsOnPlanet{100};
    SeedType seed{};
};
static_assert(offsetof(PerlinNoiseComputeSpec3D, cellSize) == 0);
static_assert(offsetof(PerlinNoiseComputeSpec3D, coordinateScale) == 4);
static_assert(offsetof(PerlinNoiseComputeSpec3D, planetRadius) == 8);
static_assert(offsetof(PerlinNoiseComputeSpec3D, dotsOnPlanet) == 16);
static_assert(offsetof(PerlinNoiseComputeSpec3D, seed) == 24);


class Generator3d {
public:
    virtual ~Generator3d() = default;

    virtual Planet<float> generatePlanet(std::size_t dots) const {
        Planet<float> planet{dots, 0.0F};
        for (std::size_t y = 0; y < dots; ++y) {
            for (std::size_t x = 0; x < dots; ++x) {
                planet.at(x, y) = noise(planet.pointUnitDir(x, y));
            }
        }

        return planet;
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
