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

    }

    void WorleyNoise2d::generateGradients() {
        for (std::size_t y = 0; y < gridHeight_; ++y) {
            for (std::size_t x = 0; x < gridWidth_; ++x) {
                gradients_.at(x, y) = hash2(x, y);
            }
        }
    }

    glm::vec2 WorleyNoise2d::featurePointAt(std::size_t i, std::size_t j) const {
        return glm::vec2{i, j} + gradients_.at(i, j);
    }

    float WorleyNoise2d::noise(std::size_t x, std::size_t y) const {
        if (x >= sampleWidth() || y >= sampleHeight()) {
            throw std::invalid_argument("Perlin sample coordinate is outside the gradient grid");
        }

        const std::size_t i0 = x / dotsPerCell_;
        const std::size_t j0 = y / dotsPerCell_;
        const float localX = static_cast<float>(x % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float localY = static_cast<float>(y % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float X = static_cast<float>(i0) + localX;
        const float Y = static_cast<float>(j0) + localY;
        const glm::vec2 point{X, Y};

        std::vector<glm::vec2> closestFeaturePoints{0};
        // Adding adjecent cell's feature points
        closestFeaturePoints.push_back(featurePointAt(i0, j0));
        if (i0 > 0 && j0 > 0) {
            closestFeaturePoints.push_back(featurePointAt(i0 - 1, j0 - 1));
        }
        if (j0 > 0) {
            closestFeaturePoints.push_back(featurePointAt(i0, j0 - 1));
        }
        if (i0 > 0) {
            closestFeaturePoints.push_back(featurePointAt(i0 - 1, j0));
        }
        if (i0 > 0 && j0 + 1 < gridHeight_) {
            closestFeaturePoints.push_back(featurePointAt(i0 - 1, j0 + 1));
        }
        if (j0 + 1 < gridHeight_) {
            closestFeaturePoints.push_back(featurePointAt(i0, j0 + 1));
        }
        if (i0 + 1 < gridWidth_ && j0 + 1 < gridHeight_) {
            closestFeaturePoints.push_back(featurePointAt(i0 + 1, j0 + 1));
        }
        if (i0 + 1 < gridWidth_) {
            closestFeaturePoints.push_back(featurePointAt(i0 + 1, j0));
        }
        if (i0 + 1 < gridWidth_ && j0 > 0) {
            closestFeaturePoints.push_back(featurePointAt(i0 + 1, j0 - 1));
        }

        // Calculate distance to each feature point
        std::vector<std::pair<float, std::size_t>> distances{};
        for (int it = 0; it < closestFeaturePoints.size(); ++it) {
            distances.emplace_back(std::sqrt(glm::dot(point, closestFeaturePoints[it])), it);
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
