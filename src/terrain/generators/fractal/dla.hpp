#pragma once

#include "terrain/generators/generator.hpp"
#include "terrain/utils/kernel.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

        DLABasic(std::size_t numSteps, HeightFunc heightFunc = defaultDLAHeightFunction(1.0F));
        DLABasic(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc = defaultDLAHeightFunction(1.0F));

        virtual HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;


    protected:
        HeightFunc heightFunc_;
        std::size_t numSteps_;


        static glm::ivec2 getRandomDirection(std::mt19937& randomDevice);
        static HeightMap<int> enumPixelsFromLeafs(const HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs);
        static HeightMap<int> enumPixelsFromSource(const HeightMap<int>& pixels, glm::ivec2 startingPos, std::unordered_set<glm::ivec2, Ivec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs);
        static bool isAdjacent(HeightMap<int>& pixels, glm::ivec2 point);
        static bool dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937& randomDevice, std::unordered_set<glm::ivec2, Ivec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs);
    private:
        float noise(std::size_t x, std::size_t y) const override;
    };

    void enumPoints(const HeightMap<int>& pixels, HeightMap<int>& dist, std::queue<glm::ivec2> q);


    template<typename VecType, typename VecHash>
    struct PointGraph {
    public:
        std::vector<std::unordered_set<std::size_t>> pointGraph{};
        std::unordered_map<VecType, int, VecHash> posIndicies{};
        std::vector<VecType> vertexPositions{};
        std::unordered_set<std::size_t> leafs{};
    };

    // const static Kernel<float> SMALL_BLUR({
    //     {false, true, false},
    //     {true,  true, true},
    //     {false, true, false}
    // }, 1.0F / 5.0F);
    const static Kernel<float> SMALL_BLUR(HeightMap<float>({
        {0.0F, 0.2F, 0.0F},
        {0.2F, 0.9F, 0.2F},
        {0.0F, 0.2F, 0.0F}
    }));



    class DLADualFilterBlur : public DLABasic {
    public:
        DLADualFilterBlur(std::size_t numSteps, HeightFunc heightFunc = defaultDLAHeightFunction(1.0F), float fill = 0.25, float jiggle = 0.021);
        DLADualFilterBlur(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc = defaultDLAHeightFunction(1.0F), float fill = 0.25, float jiggle = 0.021);

        HeightMap<float> generateHeightMap(std::size_t widthTarget, std::size_t heightTarget) const override;

        HeightMap<float> heightMapFromPixels(const HeightMap<int>& pixels) const;
        HeightMap<float> heightMapFromPointGraph(const PointGraph<glm::vec2, Vec2Hash>& praph, std::size_t width, std::size_t height) const;
        void fillPixels(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs, std::mt19937& randomDevice) const;
        static std::unordered_set<glm::ivec2, Ivec2Hash> getLeafs(const HeightMap<int>& pixels);

        // So slightly offset each points coordinates by a random vector
        // Magnitudes of offset vectors are on a normal distribution with sigma as sigma
        static void jigglePoints(PointGraph<glm::vec2, Vec2Hash>& pgraph, float sigma, std::mt19937& rd);

    protected:
        float fill_;
        float jiggle_;

        static PointGraph<glm::vec2, Vec2Hash> getRelativeCoordinates(HeightMap<int>& pixelsOld);
        static PointGraph<glm::vec2, Vec2Hash> getRelativeCoordinates(HeightMap<int>& pixelsOld, glm::ivec2 startingPos);
        static PointGraph<glm::vec2, Vec2Hash> getRelativeCoordinates(HeightMap<int>& pixelsOld, glm::ivec2 startingPos, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs);

        static std::unordered_set<glm::ivec2, Ivec2Hash> constructCrispUpscaledPixels(const PointGraph<glm::vec2, Vec2Hash>& relPointGraph, HeightMap<int>& pixels);

    private:

    };

    template<typename T, T defaultVal>
    void connectTwoPoints(HeightMap<T>& pixels, glm::ivec2 p1, glm::ivec2 p2) {
        const int deltaX = std::abs(p2.x - p1.x);
        const int stepX = p1.x < p2.x ? 1 : -1;
        const int deltaY = -std::abs(p2.y - p1.y);
        const int stepY = p1.y < p2.y ? 1 : -1;
        int error = deltaX + deltaY;

        while (true) {
            const glm::ivec2 previous = p1;
            pixels.at(p1) = defaultVal;
            if (p1 == p2) {
                break;
            }

            const int doubledError = 2 * error;
            bool steppedX = false;
            bool steppedY = false;
            if (doubledError >= deltaY) {
                error += deltaY;
                p1.x += stepX;
                steppedX = true;
            }
            if (doubledError <= deltaX) {
                error += deltaX;
                p1.y += stepY;
                steppedY = true;
            }

            if (steppedX && steppedY) {
                pixels.at(previous.x, p1.y) = defaultVal;
            }
        }
    }

    template<typename T> requires requires (T t) { t = 1; }
    void connectTwoPoints(HeightMap<T>& pixels, glm::ivec2 p1, glm::ivec2 p2) {
        connectTwoPoints<T, 1>(pixels, p1, p2);
    }

    template<typename T>
    void upscaleHeightmapWeightedLerp(const HeightMap<T>& pixelsOld, HeightMap<T>& pixelsNew) {
        assert(
            pixelsOld.width() <= pixelsNew.width() && pixelsOld.height() <= pixelsNew.height()
            && "To upscale new heightmap needs to be larger than the old one"
        );
        if (pixelsOld.width() == pixelsNew.width() && pixelsOld.height() == pixelsNew.height()) {
            pixelsNew = pixelsOld;
            return;
        }
        for (std::size_t yNew = 0; yNew < pixelsNew.height(); ++yNew) {
            for (std::size_t xNew = 0; xNew < pixelsNew.width(); ++xNew) {
                const float relX = static_cast<float>(xNew) / static_cast<float>(pixelsNew.width() - 1);
                const float relY = static_cast<float>(yNew) / static_cast<float>(pixelsNew.height() - 1);

                const float xOld = relX * static_cast<float>(pixelsOld.width() - 1);
                const float yOld = relY * static_cast<float>(pixelsOld.height() - 1);

                const auto x1 = static_cast<std::size_t>(std::floor(xOld));
                const auto y1 = static_cast<std::size_t>(std::floor(yOld));
                const std::size_t x2 = std::min(x1 + 1, pixelsOld.width() - 1);
                const std::size_t y2 = std::min(y1 + 1, pixelsOld.height() - 1);

                const float tx = xOld - static_cast<float>(x1);
                const float ty = yOld - static_cast<float>(y1);

                const auto top = lerp(pixelsOld.at(x1, y1), pixelsOld.at(x2, y1), tx);
                const auto bottom = lerp(pixelsOld.at(x1, y2), pixelsOld.at(x2, y2), tx);
                pixelsNew.at(xNew, yNew) = lerp(top, bottom, ty);
            }
        }

    }

}
