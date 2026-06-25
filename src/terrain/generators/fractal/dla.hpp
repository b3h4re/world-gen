#pragma once

#include "terrain/generators/generator.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <unordered_set>

namespace wgen {

    // Helper for getting random points
    class RandomGridPoints {
    public:
        using Point = glm::vec<2, int>;

        RandomGridPoints();
        RandomGridPoints(std::uint32_t seed);

        Point next(std::size_t width, std::size_t height);

        void reset(); // Resets random device
        void setSeed(std::uint32_t seed);


    private:
        std::size_t width_;
        std::size_t height_;
        // std::vector<Point> points_;
        // std::size_t index_ = 0;
        std::uint32_t seed_{};
        std::mt19937 random_{};
    };


    constexpr glm::ivec2 DIRECTIONS[4] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    /*
    Grid here will have the center of coordinates in its center. Because practically it doesnt matter where we start.
    */
    class DLABasic : public Generator {
    public:
        using HeightFunc = float (*)(int); // Function whic receives int of point and returns height
        DLABasic(std::size_t numSteps, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);
        DLABasic(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;


    protected:
        HeightFunc heightFunc_;
        std::size_t numSteps_;


        static glm::ivec2 getRandomDirection(std::mt19937& randomDevice);
        void enumPixelsFromLeafs(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const;
        void enumPixelsFromSource(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const;
        bool isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) const;
        void dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937& randomDevice, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const;
    private:
        float noise(std::size_t x, std::size_t y) const override;
    };


}
