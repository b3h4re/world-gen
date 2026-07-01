#pragma once

#include "terrain/generators/generator.hpp"
#include "terrain/utils/hash_random.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>


namespace wgen {


    template <typename GenClass>
    requires valid_generator<GenClass>
    class OctaveGenerator : public Generator  {
    public:
        static constexpr std::size_t DEFAULT_NUM_OCTAVES = 3;
        static constexpr float DEFAULT_LACUNARITY = 2.5F;
        static constexpr float DEFAULT_PERSISTANCE = 0.5F;

        explicit OctaveGenerator(
            SeedType seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, SeedType>
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
            SeedType seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, std::size_t, std::size_t, SeedType>
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
            SeedType seed,
            std::size_t numOctaves = DEFAULT_NUM_OCTAVES,
            float lacunarity = DEFAULT_LACUNARITY,
            float persistance = DEFAULT_PERSISTANCE)
        requires std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, SeedType>
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

        void setSeed(SeedType newSeed) override {
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

        void rebuildOctaves(SeedType seed) {
            octaves.clear();
            octaves.reserve(numOctaves_);

            SeedType octaveSeed = seed;
            for (std::size_t octave = 0; octave < numOctaves_; ++octave) {
                makeOctave(octave, octaveSeed);
                octaveSeed = hashSeed(octaveSeed);
            }
        }

        void makeOctave(std::size_t octave, SeedType seed) {
            switch (constructorKind_) {
                case ConstructorKind::SeedOnly:
                    if constexpr (std::constructible_from<GenClass, SeedType>) {
                        octaves.push_back(std::make_unique<GenClass>(seed));
                    } else {
                        throw std::logic_error("generator does not support seed-only construction");
                    }
                    break;
                case ConstructorKind::Grid:
                    if constexpr (std::constructible_from<GenClass, std::size_t, std::size_t, SeedType>) {
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
                    if constexpr (std::constructible_from<GenClass, std::size_t, std::size_t, std::size_t, SeedType>) {
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
