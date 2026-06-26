#pragma once

#include "terrain/generators/generator.hpp"
#include "terrain/utils/kernel.hpp"

#include <unordered_set>
#include <iostream>

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

    const static Kernel<float> SMALL_BLUR({
        {false, true, false},
        {true,  true, true},
        {false, true, false}
    }, 1.0F / 5.0F);


    class DLADualFilterBlur : public DLABasic {
    public:
        DLADualFilterBlur(std::size_t numSteps, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);
        DLADualFilterBlur(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc = defaultDLAHeightFunction<1.0F>);

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;

    protected:
        HeightFunc heightFunc_;
        std::size_t numSteps_;

        static PointGraph<glm::vec2> getRelativeCoordinates(HeightMap<int>& pixelsOld, glm::ivec2 startingPos);

        static void constructUpscaledPixels(PointGraph<glm::vec2>& relPointGraph, HeightMap<int>& pixels);

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
            pixels.at(p1) = defaultVal;
            if (p1 == p2) {
                break;
            }

            const int doubledError = 2 * error;
            if (doubledError >= deltaY) {
                error += deltaY;
                p1.x += stepX;
            }
            if (doubledError <= deltaX) {
                error += deltaX;
                p1.y += stepY;
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
        for (std::size_t xNew = 0; xNew < pixelsNew.width(); ++xNew) {
            for (std::size_t yNew = 0; yNew < pixelsNew.height(); ++yNew) {
                float relX = static_cast<float>(xNew) / static_cast<float>(pixelsNew.width() - 1);
                float relY = static_cast<float>(yNew) / static_cast<float>(pixelsNew.height() - 1);

                float xOld = relX * static_cast<float>(pixelsOld.width() - 1);
                float yOld = relY * static_cast<float>(pixelsOld.height() - 1);

                std::size_t x1 = static_cast<std::size_t>(std::floor(xOld));
                std::size_t x2 = std::min(
                    std::max(x1+1, static_cast<std::size_t>(std::ceil(xOld))),
                    pixelsOld.width() - 1
                );
                std::size_t y1 = static_cast<std::size_t>(std::floor(yOld));
                std::size_t y2 = std::min(
                    std::max(y1+1, static_cast<std::size_t>(std::ceil(yOld))),
                    pixelsOld.height() - 1
                );
                // So now we have 4 pixels in between which lies our new one (x1, y1), (x1, y2), (x2, y1), (x2, y2)
                glm::ivec2 p1{x1, y1}, p2{x1, y2}, p3{x2, y1}, p4{x2, y2};
                std::vector<glm::ivec2> nearestPixels{p1, p2, p3, p4};
                std::vector<std::pair<float, glm::ivec2>> distances{};
                float totalDist{0};
                distances.reserve(4);
                for (int i = 0; i < 4; ++i) {
                    float dist = minkowskiDistance({xOld, yOld}, nearestPixels[i], 2.0F);
                    totalDist += dist;
                    distances.emplace_back(dist, nearestPixels[i]);
                }

                float heightVal{0};
                float totalWeight{0};
                for (int i = 0; i < 4; ++i) {
                    float weight = (totalDist - distances[i].first) / totalDist;
                    totalWeight += weight;
                    heightVal += weight * pixelsOld.at(distances[i].second);
                }
                heightVal /= totalWeight;
                pixelsNew.at(xNew, yNew) = heightVal;
            }
        }

    }

}
