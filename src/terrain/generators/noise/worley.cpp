#include "worley.hpp"
#include "terrain/utils/hash_random.hpp"


#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>


namespace wgen {

    std::uint64_t worleyFeatureHash(
            SeedType seed,
            std::ptrdiff_t cellX,
            std::ptrdiff_t cellY,
            std::size_t featureIndex,
            std::uint32_t component) {
        std::uint64_t hash = splitmix64(seed);
        hash = splitmix64(hash ^ splitmix64(static_cast<std::uint32_t>(cellX)));
        hash = splitmix64(hash ^ splitmix64(static_cast<std::uint32_t>(cellY)));
        hash = splitmix64(hash ^ splitmix64(static_cast<std::uint64_t>(featureIndex)));
        hash = splitmix64(hash ^ splitmix64(component));
        return hash;
    }

    glm::vec2 worleyFeaturePoint(SeedType seed, std::ptrdiff_t cellX, std::ptrdiff_t cellY, std::size_t featureIndex) {
        return {
            toUnitFloat(worleyFeatureHash(seed, cellX, cellY, featureIndex, 0)),
            toUnitFloat(worleyFeatureHash(seed, cellX, cellY, featureIndex, 1)),
        };
    }

    WorleyNoise2d::WorleyNoise2d(std::size_t dotsPerCell, float p, std::size_t numPoints)
    : WorleyNoise2d{dotsPerCell, std::random_device{}(), p, numPoints} {}

    WorleyNoise2d::WorleyNoise2d(std::size_t dotsPerCell,
                                 SeedType seed, float p, std::size_t numPoints)
    : dotsPerCell_{dotsPerCell}, p_{p}, numPoints_{numPoints} {
        if (p_ <= 0.0F) {
            throw std::invalid_argument("Worley distance exponent must be positive");
        }
        if (numPoints_ == 0) {
            throw std::invalid_argument("Worley feature point count must be at least one");
        }
    }

    // void WorleyNoise2d::generateGradients() {
    //     std::mt19937 random{getSeed()};

    //     for (std::size_t y = 0; y < gridHeight_; ++y) {
    //         for (std::size_t x = 0; x < gridWidth_; ++x) {
    //             featurePoints_.at(x, y).clear();
    //             featurePoints_.at(x, y).reserve(numPoints_);
    //             for (std::size_t n = 0; n < numPoints_; ++n) {
    //                 featurePoints_.at(x, y).emplace_back(
    //                     (glm::vec2{1, 1} + hash2(static_cast<int>(x + random()), static_cast<int>(y + random()))) / 2.0F
    //                 );
    //             }
    //         }
    //     }
    // }

    std::vector<glm::vec2> WorleyNoise2d::featurePointsAt(std::size_t i, std::size_t j) const {
        std::vector<glm::vec2> points{};
        points.reserve(numPoints_);

        for (std::size_t n = 0; n < numPoints_; ++n) {
            points.emplace_back(worleyFeaturePoint(
                getSeed(),
                static_cast<std::ptrdiff_t>(i),
                static_cast<std::ptrdiff_t>(j),
                n));
        }

        return points;
    }

    std::size_t WorleyNoise2d::wrapSignedIndex(std::ptrdiff_t index, std::size_t size) {
        const auto signedSize = static_cast<std::ptrdiff_t>(size);
        const auto wrapped = index % signedSize;

        if (wrapped < 0) {
            return static_cast<std::size_t>(wrapped + signedSize);
        }

        return static_cast<std::size_t>(wrapped);
    }

    std::vector<glm::vec2> WorleyNoise2d::deltaToAdjacentFeaturePoints(std::size_t x, std::size_t y, std::ptrdiff_t i, std::ptrdiff_t j) const {
        const auto cellX = static_cast<std::ptrdiff_t>(x) + i;
        const auto cellY = static_cast<std::ptrdiff_t>(y) + j;

        std::vector<glm::vec2> points{};
        points.reserve(numPoints_);
        for (std::size_t n = 0; n < numPoints_; ++n) {
            points.emplace_back(glm::vec2{cellX, cellY} + worleyFeaturePoint(getSeed(), cellX, cellY, n));
        }

        return points;
    }

    float WorleyNoise2d::noise(std::size_t x, std::size_t y) const {

        const std::size_t i0 = x / dotsPerCell_;
        const std::size_t j0 = y / dotsPerCell_;
        const float localX = static_cast<float>(x % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float localY = static_cast<float>(y % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float X = static_cast<float>(i0) + localX;
        const float Y = static_cast<float>(j0) + localY;
        const glm::vec2 point{X, Y};

        // Adding adjecent cell's feature points with looping back to the end
        std::vector<glm::vec2> featurePointDeltas{};
        for (std::ptrdiff_t i = -1; i <= 1; ++i) {
            for (std::ptrdiff_t j = -1; j <= 1; ++j) {
                for (auto elem : deltaToAdjacentFeaturePoints(i0, j0, i, j)) {
                    featurePointDeltas.emplace_back(elem - point);
                }
            }
        }

        // Calculate distance to each feature point
        std::vector<std::pair<float, std::size_t>> distances{};
        for (int it = 0; it < featurePointDeltas.size(); ++it) {
            distances.emplace_back(minkowskiDistance(featurePointDeltas[it], {0, 0}, p_), it);
        }

        // Sort by distance, first element is the closest
        std::sort(
            distances.begin(),
            distances.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.first < rhs.first;
            }
        );

        return distances[1].first - distances[0].first;
    }

    GeneratorCapabilities WorleyNoise2d::capabilities() const {
        return {
            .cpu = true,
            .vulkanCompute = true,
        };
    }

    std::string WorleyNoise2d::compShader() const {
        if (numPoints_ < MIN_GPU_FEATURE_POINT_COUNT || numPoints_ > MAX_GPU_FEATURE_POINT_COUNT) {
            throw std::runtime_error("Worley GPU shader supports 1 to 8 feature points");
        }

        return "worley_noise_" + std::to_string(numPoints_);
    }

    std::size_t WorleyNoise2d::specSize() const {
        return sizeof(WorleyNoiseComputeSpec);
    }

}
