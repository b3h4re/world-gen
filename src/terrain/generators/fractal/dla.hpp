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

        virtual HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;


    protected:
        HeightFunc heightFunc_;
        std::size_t numSteps_;


        static glm::ivec2 getRandomDirection(std::mt19937& randomDevice);
        static void enumPixelsFromLeafs(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs);
        static void enumPixelsFromSource(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs);
        static bool isAdjacent(HeightMap<int>& pixels, glm::ivec2 point);
        static bool dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937& randomDevice, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs);
    private:
        float noise(std::size_t x, std::size_t y) const override;
    };


    template<typename VecType>
    struct PointGraph {
    public:
        std::vector<std::unordered_set<std::size_t>> pointGraph{};
        std::unordered_map<VecType, int, Vec2Hash> posIndicies{};
        std::vector<VecType> vertexPositions{};
    };


    class DLADualFilterBlur : public DLABasic {
    public:
        DLADualFilterBlur(std::size_t numSteps, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);
        DLADualFilterBlur(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;

    protected:
        HeightFunc heightFunc_;
        std::size_t numSteps_;

        static PointGraph<glm::vec2> getRelativeCoordinates(HeightMap<int>& pixelsOld, glm::ivec2 startingPos);

        static void connectTwoPoints(HeightMap<int>& pixels, glm::ivec2 p1, glm::ivec2 p2);

        static void constructUpscaledPixels(PointGraph<glm::vec2>& relPointGraph, HeightMap<int>& pixels);

    private:

    };


}
