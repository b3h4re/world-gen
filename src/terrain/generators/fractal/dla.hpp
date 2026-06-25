#pragma once

#include "terrain/generators/generator.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <unordered_set>

namespace wgen {
    namespace {
        struct Vec2Hash {
            std::size_t operator()(const glm::ivec2& v) const noexcept {
                std::size_t h1 = std::hash<int>{}(v.x);
                std::size_t h2 = std::hash<int>{}(v.y);

                // hash combine
                return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            }
        };
    }

    // Helper for getting random points
    class RandomGridPoints {
    public:
        using Point = glm::vec<2, int>;

        RandomGridPoints(std::size_t width, std::size_t height);
        RandomGridPoints(std::size_t width, std::size_t height, std::uint32_t seed);

        Point next();
        // bool empty() const;
        void clear(); // Clears everything and resets
        void reset(); // Resets random device
        void setSeed(std::uint32_t seed);
        template <typename... Args>
        void exclude(Args&&... excludedPoints) {
            (excluded_points.insert(std::forward<Args>(excludedPoints)), ...);
        }

    private:
        std::size_t width_;
        std::size_t height_;
        // std::vector<Point> points_;
        // std::size_t index_ = 0;
        std::uint32_t seed_{};
        std::mt19937 random_{};

        std::unordered_set<Point, Vec2Hash> excluded_points{};
    };


    constexpr glm::ivec2 DIRECTIONS[4] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    /*
    Grid here will have the center of coordinates in its center. Because practically it doesnt matter where we start.
    */
    class DLABasic : public Generator {
    public:
        DLABasic(std::size_t gridWidth, std::size_t gridHeight, std::size_t numSteps);
        DLABasic(std::size_t gridWidth, std::size_t gridHeight, std::size_t numSteps, std::uint32_t seed);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;


    private:
        std::size_t gridWidth_;
        std::size_t gridHeight_;
        std::size_t numSteps_;
        glm::vec<2, int> startingPos_;


        static glm::ivec2 getRandomDirection(std::mt19937 randomDevice);
        bool isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) const;
        void dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937 randomDevice) const;
    };


}
