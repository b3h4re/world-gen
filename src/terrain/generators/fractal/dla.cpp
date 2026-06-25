#include "dla.hpp"


#include <unordered_map>
#include <queue>
#include <iostream>


namespace wgen {

    RandomGridPoints::RandomGridPoints() : RandomGridPoints{std::random_device{}()} {}
    RandomGridPoints::RandomGridPoints(std::uint32_t seed) : seed_{seed} {
        this->reset();
    }

    void RandomGridPoints::reset() {
        random_ = std::mt19937(seed_);
    }

    void RandomGridPoints::setSeed(std::uint32_t newSeed) {
        seed_ = newSeed;
        this->reset();
    }

    RandomGridPoints::Point RandomGridPoints::next(std::size_t width, std::size_t height) {
        std::uniform_int_distribution<int> x_dist(0, static_cast<int>(width - 1));
        std::uniform_int_distribution<int> y_dist(0, static_cast<int>(height - 1));
        return {x_dist(random_), y_dist(random_)};
    }


    DLABasic::DLABasic(std::size_t gridWidth, std::size_t gridHeight, std::size_t numSteps)
        : DLABasic{gridWidth, gridHeight, numSteps, std::random_device{}()} {}
    DLABasic::DLABasic(std::size_t gridWidth, std::size_t gridHeight, std::size_t numSteps, std::uint32_t seed)
        : gridWidth_{gridWidth}, gridHeight_{gridHeight}, numSteps_{numSteps},
        startingPos_{gridWidth / 2, gridHeight / 2} {
        setSeed(seed);
    }

    HeightMap<float> DLABasic::generateHeightMap(std::size_t width, std::size_t height) const {
        HeightMap<int> pixels{gridWidth_, gridHeight_};
        pixels.at(startingPos_) = 1;
        RandomGridPoints points{getSeed()};
        HeightMap<float> res{width, height};

        std::mt19937 random{getSeed()};
        std::unordered_set<glm::ivec2, Vec2Hash> placedPoints{};
        for (std::size_t i = 0; i < numSteps_; ++i) {
            std::cout << "DLA steps: " << i + 1 << "/" << numSteps_ << "\n";
            dlaStep(pixels, points, random, placedPoints);
        }

        std::unordered_set<glm::ivec2, Vec2Hash> leafs{};
        findLeafs(pixels, leafs);
        std::unordered_set<std::pair<glm::ivec2, glm::ivec2>, Vec2Vec2Hash> counted{};
        std::queue<std::pair<glm::ivec2, glm::ivec2>> q{};
        for (const auto& pos : leafs) {
            q.push({pos, pos});
        }

        // Basically bfs until we visit all generated points in cluster
        while (q.size() > 0) {
            glm::ivec2 pos = q.front().first;
            glm::ivec2 prevPos = q.front().second;
            counted.insert({pos, prevPos});
            counted.insert({prevPos, pos});
            q.pop();

            if (pos != prevPos) {
                pixels.at(pos) = std::max(pixels.at(pos), pixels.at(prevPos) + 1);
            }

            for (const auto& dir : DIRECTIONS) {
                if (!isInside(pos, dir, pixels.width(), pixels.height()) || leafs.contains(pos + dir)
                    || !placedPoints.contains(pos + dir)
                    || counted.contains({pos, pos+dir}) || counted.contains({pos + dir, pos})) {
                    continue;
                }
                if (pos != prevPos && pixels.at(pos) <= pixels.at(pos + dir)) {
                    continue;
                }
                q.push({pos + dir, pos});
            }
        }

        for (const auto& p : placedPoints) {
            res.at(p) = heightFunc(pixels.at(p));
        }

        return res;
    }

    void DLABasic::findLeafs(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const {
        for (std::size_t x = 0; x < pixels.width(); ++x) {
            for (std::size_t y = 0; y < pixels.height(); ++y) {
                glm::ivec2 pos{x, y};
                if (pixels.at(pos) != 1) {
                    continue;
                }
                int numNeighboors{0};
                for (auto& dir : DIRECTIONS) {
                    if (!isInside(pos, dir, pixels.width(), pixels.height())) { continue; }
                    if (pixels.at(pos + dir) == 1) {
                        numNeighboors++;
                    }
                }
                if (numNeighboors <= 1) {
                    leafs.insert(pos);
                }
            }
        }
    }

    glm::ivec2 DLABasic::getRandomDirection(std::mt19937 randomDevice) {
        return DIRECTIONS[randomDevice() % 4];
    }

    bool DLABasic::isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) const {
        for (int i = 0; i < 4; ++i) {
            glm::ivec2 adjacentPosition = point + DIRECTIONS[i];
            if (!isInside(adjacentPosition, {0,0}, gridWidth_, gridHeight_)) {
                continue;
            }
            if (pixels.at(adjacentPosition) == 1) {
                return true;
            }
        }
        return false;
    }

    void DLABasic::dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937& randomDevice, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints) const {
        std::size_t randomSizeX = std::min(3 + placedPoints.size(), pixels.width());
        std::size_t randomSizeY = std::min(3 + placedPoints.size(), pixels.height());
        glm::ivec2 centerOfRand{randomSizeX/2, randomSizeY/2};
        glm::ivec2 point = startingPos_ + points.next(randomSizeX, randomSizeY) - centerOfRand;

        // std::cout << "Start position: (" << startingPos_.x << ", " << startingPos_.y << ")\n";
        glm::ivec2 corner1 = startingPos_ - glm::ivec2{randomSizeX/2 + 1, randomSizeY/2 + 1};
        glm::ivec2 corner2 = startingPos_ + glm::ivec2{randomSizeX/2 + 1, randomSizeY/2 + 1};
        // std::cout << "Corner 1: (" << corner1.x << ", " << corner1.y << ")\n";
        // std::cout << "Corner 2: (" << corner2.x << ", " << corner2.y << ")\n";
        // std::cout << "Start point: (" << point.x << ", " << point.y << ")\n";
        while (!isAdjacent(pixels, point)) {
            glm::ivec2 dir = DIRECTIONS[randomDevice() % 4];
            if (!isInside(point, dir, pixels.width(), pixels.height())) {
                continue;
            }
            if (!isInsideRectangle(point, dir, corner1, corner2)) {
                continue;
            }
            point += dir;
            // std::cout << "Mid point: (" << point.x << ", " << point.y << ")\n";
        }
        // std::cout << "End point: (" << point.x << ", " << point.y << ")\n";
        points.exclude(point);
        pixels.at(point) = 1;
        placedPoints.insert(point);
    }

    float DLABasic::noise(std::size_t x, std::size_t y) const {
        return 0.0F;
    }

}
