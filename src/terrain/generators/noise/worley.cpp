#include "worley.hpp"


#include <algorithm>
#include <vector>
#include <utility>


namespace wgen {

    WorleyNoise2d::WorleyNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell)
    : WorleyNoise2d{gridWidth, gridHeight, dotsPerCell, std::random_device{}()} {}

    WorleyNoise2d::WorleyNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell,
                                 std::uint32_t seed)
    : GradientNoise{gridWidth, gridHeight, dotsPerCell, seed} {
        generateGradients();
    }

    void WorleyNoise2d::generateGradients() {
        for (std::size_t y = 0; y < gridHeight_; ++y) {
            for (std::size_t x = 0; x < gridWidth_; ++x) {
                // hash2 gives values from [-1, 1)
                // So add 1 and divide by two to get [0, 1)
                gradients_.at(x, y) = (
                    glm::vec2{1, 1} + hash2(
                        static_cast<int>(x + getSeed()),
                        static_cast<int>(y + getSeed())
                    )
                ) / 2.0F;
            }
        }
    }

    glm::vec2 WorleyNoise2d::featurePointAt(std::size_t i, std::size_t j) const {
        return glm::vec2{i, j} + gradients_.at(i, j);
    }

    std::size_t WorleyNoise2d::wrapSignedIndex(std::ptrdiff_t index, std::size_t size) {
        const auto signedSize = static_cast<std::ptrdiff_t>(size);
        const auto wrapped = index % signedSize;

        if (wrapped < 0) {
            return static_cast<std::size_t>(wrapped + signedSize);
        }

        return static_cast<std::size_t>(wrapped);
    }

    glm::vec2 WorleyNoise2d::deltaToAdjacentFeaturePoint(std::size_t x, std::size_t y, std::ptrdiff_t i, std::ptrdiff_t j) const {
        const auto cellX = static_cast<std::ptrdiff_t>(x) + i;
        const auto cellY = static_cast<std::ptrdiff_t>(y) + j;
        const auto wrappedX = wrapSignedIndex(cellX, gridWidth_);
        const auto wrappedY = wrapSignedIndex(cellY, gridHeight_);

        return glm::vec2{cellX, cellY} + gradients_.at(wrappedX, wrappedY);
    }

    float WorleyNoise2d::noise(std::size_t x, std::size_t y) const {
        if (x >= sampleWidth() || y >= sampleHeight()) {
            throw std::invalid_argument("Worley sample coordinate is outside the gradient grid");
        }

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
                featurePointDeltas.emplace_back(
                    deltaToAdjacentFeaturePoint(i0, j0, i, j) - point
                );
            }
        }

        // Calculate distance to each feature point
        std::vector<std::pair<float, std::size_t>> distances{};
        for (int it = 0; it < featurePointDeltas.size(); ++it) {
            distances.emplace_back(std::sqrt(glm::dot(featurePointDeltas[it], featurePointDeltas[it])), it);
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

}
