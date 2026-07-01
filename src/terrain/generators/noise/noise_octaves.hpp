#pragma once

#include "terrain/generators/generator.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>


namespace wgen {

    template <typename GenClass>
    concept has_noise_function = requires (const GenClass& gen, std::size_t x, std::size_t y) {
        { gen.noise(x, y) } -> std::convertible_to<float>;
    };


    template <typename GenClass>
    concept valid_generator =
        std::derived_from<GenClass, Generator> &&
        has_noise_function<GenClass> &&
        (
            std::constructible_from<GenClass, std::uint32_t> ||
            std::constructible_from<GenClass, std::size_t, std::size_t, std::uint32_t> ||
            std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, std::uint32_t>
        );


    template <typename GenClass>
    requires valid_generator<GenClass>
    class OctaveGenerator : public Generator  {
    public:
        static constexpr std::size_t DEFAULT_NUM_OCTAVES = 3;
        static constexpr float DEFAULT_LACUNARITY = 2.5F;
        static constexpr float DEFAULT_PERSISTANCE = 0.5F;

        explicit OctaveGenerator(
            std::uint32_t seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, std::uint32_t>
            : constructorKind_{ConstructorKind::SeedOnly},
              numOctaves_{numOctaves},
              lacunarity_{lacunarity},
              persistance_{persistance} {
            validateConfig();
            setSeed(seed);
        }

        OctaveGenerator(
            std::size_t width,
            std::size_t height,
            std::uint32_t seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, std::size_t, std::size_t, std::uint32_t>
            : constructorKind_{ConstructorKind::Grid},
              width_{width},
              height_{height},
              numOctaves_{numOctaves},
              lacunarity_{lacunarity},
              persistance_{persistance} {
            validateConfig();
            setSeed(seed);
        }

        OctaveGenerator(
            std::size_t width,
            std::size_t height,
            std::size_t dots,
            std::uint32_t seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, std::uint32_t>
            : constructorKind_{ConstructorKind::GridWithDots},
              width_{width},
              height_{height},
              dots_{dots},
              numOctaves_{numOctaves},
              lacunarity_{lacunarity},
              persistance_{persistance} {
            validateConfig();
            setSeed(seed);
        }

        void setSeed(std::uint32_t newSeed) override {
            Generator::setSeed(newSeed);
            rebuildOctaves(newSeed);
        }

        float noise(std::size_t x, std::size_t y) const override {
            float resNoise{0};
            for (std::size_t i = 0; i < octaves.size(); ++i) {
                resNoise += amplitude(i) * octaves[i]->noise(sampleCoordinate(x, i), sampleCoordinate(y, i));
            }
            return resNoise;
        }


    private:
        enum class ConstructorKind {
            SeedOnly,
            Grid,
            GridWithDots
        };

        ConstructorKind constructorKind_;
        std::size_t width_{};
        std::size_t height_{};
        std::size_t dots_{};
        std::size_t numOctaves_{};
        float lacunarity_{};
        float persistance_{};
        std::vector<std::unique_ptr<Generator>> octaves;

        float frequency(std::size_t octave) const {
            return std::pow(lacunarity_, static_cast<float>(octave));
        }

        float amplitude(std::size_t octave) const {
            return std::pow(persistance_, static_cast<float>(octave));
        }

        std::size_t octaveScale(std::size_t value, std::size_t octave) const {
            float scaled = static_cast<float>(value) * frequency(octave);
            return static_cast<std::size_t>(scaled);
        }

        std::size_t sampleCoordinate(std::size_t coordinate, std::size_t octave) const {
            return octaveScale(coordinate, octave);
        }

        void validateConfig() const {
            if (numOctaves_ == 0) {
                throw std::invalid_argument("octave count must be at least one");
            }
            if (lacunarity_ <= 0.0F) {
                throw std::invalid_argument("octave lacunarity must be positive");
            }
            if (persistance_ < 0.0F) {
                throw std::invalid_argument("octave persistance must be non-negative");
            }
        }

        void rebuildOctaves(std::uint32_t seed) {
            octaves.clear();
            octaves.reserve(numOctaves_);

            std::uint32_t octaveSeed = seed;
            for (std::size_t octave = 0; octave < numOctaves_; ++octave) {
                makeOctave(octave, octaveSeed);
                octaveSeed = hashSeed(octaveSeed);
            }
        }

        void makeOctave(std::size_t octave, std::uint32_t seed) {
            switch (constructorKind_) {
                case ConstructorKind::SeedOnly:
                    if constexpr (std::constructible_from<GenClass, std::uint32_t>) {
                        octaves.push_back(std::make_unique<GenClass>(seed));
                    } else {
                        throw std::logic_error("generator does not support seed-only construction");
                    }
                    break;
                case ConstructorKind::Grid:
                    if constexpr (std::constructible_from<GenClass, std::size_t, std::size_t, std::uint32_t>) {
                        octaves.push_back(std::make_unique<GenClass>(
                            octaveScale(width_, octave),
                            octaveScale(height_, octave),
                            seed
                        ));
                    } else {
                        throw std::logic_error("generator does not support grid construction");
                    }
                    break;
                case ConstructorKind::GridWithDots:
                    if constexpr (std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, std::uint32_t>) {
                        octaves.push_back(std::make_unique<GenClass>(
                            octaveScale(width_, octave),
                            octaveScale(height_, octave),
                            dots_,
                            seed
                        ));
                    } else {
                        throw std::logic_error("generator does not support grid-with-dots construction");
                    }
                    break;
            }
        }
    };

}
