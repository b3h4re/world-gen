#include "dla.hpp"


#include <unordered_map>
#include <queue>
#include <iostream>
#include <vector>


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


    DLABasic::DLABasic(std::size_t numSteps, HeightFunc heightFunc)
        : DLABasic{numSteps, std::random_device{}(), heightFunc} {}
    DLABasic::DLABasic(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc)
        : numSteps_{numSteps}, heightFunc_{heightFunc} {
        setSeed(seed);
    }

    HeightMap<float> DLABasic::generateHeightMap(std::size_t width, std::size_t height) const {
        HeightMap<int> pixels{width, height};
        glm::ivec2 startingPos{width/2, height/2};
        pixels.at(startingPos) = 1;
        RandomGridPoints points{getSeed()};
        HeightMap<float> res{width, height};

        std::mt19937 random{getSeed()};
        std::unordered_set<glm::ivec2, Vec2Hash> placedPoints{};
        placedPoints.insert(startingPos);
        std::unordered_set<glm::ivec2, Vec2Hash> leafs{};
        leafs.insert(startingPos);
        for (std::size_t i = 0; i < numSteps_; ++i) {
            std::cout << "DLA steps: " << i + 1 << "/" << numSteps_ << "\n";
            if (!dlaStep(pixels, points, random, placedPoints, leafs)) {
                break;
            }
        }
        std::cout << "DLA steps finished. Placed points: " << placedPoints.size() << "\n";
        // findLeafs(pixels, leafs);
        enumPixelsFromLeafs(pixels, placedPoints, leafs);


        for (const auto& p : placedPoints) {
            res.at(p) = heightFunc_(pixels.at(p));
        }

        return res;
    }

    void DLABasic::enumPixelsFromSource(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const {
        std::unordered_set<std::pair<glm::ivec2, glm::ivec2>, Vec2Vec2Hash> counted{};
        std::queue<std::pair<glm::ivec2, glm::ivec2>> q{};
        glm::ivec2 startingPos{pixels.width()/2, pixels.height()/2};
        q.push({startingPos, startingPos});

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
                if (!isInside(pos, dir, pixels.width(), pixels.height()) || pos + dir == startingPos
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
    }


    void DLABasic::enumPixelsFromLeafs(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const {
        std::unordered_set<std::pair<glm::ivec2, glm::ivec2>, Vec2Vec2Hash> counted{};
        std::queue<std::pair<glm::ivec2, glm::ivec2>> q{};
        for (const auto& pos : leafs) {
            q.push({pos, pos});
        }
        int maxNum{0};

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
            maxNum = std::max(maxNum, pixels.at(pos));

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
            pixels.at(p) = maxNum + 1 - pixels.at(p);
        }
    }

    glm::ivec2 DLABasic::getRandomDirection(std::mt19937& randomDevice) {
        return DIRECTIONS[randomDevice() % 4];
    }

    bool DLABasic::isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) const {
        for (int i = 0; i < 4; ++i) {
            glm::ivec2 adjacentPosition = point + DIRECTIONS[i];
            if (!isInside(adjacentPosition, {0,0}, pixels.width(), pixels.height())) {
                continue;
            }
            if (pixels.at(adjacentPosition) == 1) {
                return true;
            }
        }
        return false;
    }

    bool DLABasic::dlaStep(
            HeightMap<int>& pixels,
            RandomGridPoints& points,
            std::mt19937& randomDevice,
            std::unordered_set<glm::ivec2, Vec2Hash>& placedPoints,
            std::unordered_set<glm::ivec2, Vec2Hash>& leafs) const
    {
        std::size_t randomSizeX = std::min(3 + placedPoints.size(), pixels.width());
        std::size_t randomSizeY = std::min(3 + placedPoints.size(), pixels.height());
        glm::ivec2 centerOfRand{randomSizeX/2, randomSizeY/2};
        glm::ivec2 startingPos{pixels.width()/2, pixels.height()/2};
        glm::ivec2 point{};
        bool foundPoint{false};
        constexpr std::size_t maxRandomAttempts{10};
        for (std::size_t attempt = 0; attempt < maxRandomAttempts; ++attempt) {
            point = startingPos + points.next(randomSizeX, randomSizeY) - centerOfRand;
            if (pixels.at(point) == 0) {
                foundPoint = true;
                break;
            }
        }

        glm::ivec2 corner1 = startingPos - glm::ivec2{randomSizeX/2 + 1, randomSizeY/2 + 1};
        glm::ivec2 corner2 = startingPos + glm::ivec2{randomSizeX/2 + 1, randomSizeY/2 + 1};
        if (!foundPoint) {
            std::vector<glm::ivec2> availablePoints;
            availablePoints.reserve(pixels.width() * pixels.height() - placedPoints.size());
            for (std::size_t y = 0; y < pixels.height(); ++y) {
                for (std::size_t x = 0; x < pixels.width(); ++x) {
                    if (pixels.at(x, y) == 0) {
                        availablePoints.emplace_back(
                            static_cast<int>(x),
                            static_cast<int>(y)
                        );
                    }
                }
            }

            if (availablePoints.empty()) {
                return false;
            }

            std::uniform_int_distribution<std::size_t> pointDist{
                0,
                availablePoints.size() - 1
            };
            point = availablePoints[pointDist(randomDevice)];
            corner1 = {0, 0};
            corner2 = {
                static_cast<int>(pixels.width() - 1),
                static_cast<int>(pixels.height() - 1)
            };
        }

        while (!isAdjacent(pixels, point)) {
            glm::ivec2 dir = getRandomDirection(randomDevice);
            if (!isInside(point, dir, pixels.width(), pixels.height())) {
                continue;
            }
            if (!isInsideRectangle(point, dir, corner1, corner2)) {
                continue;
            }
            point += dir;
        }

        pixels.at(point) = 1;
        placedPoints.insert(point);

        int numNeighboorsSelf{0};
        for (const auto& dir : DIRECTIONS) {
            if (!isInside(point, dir, pixels.width(), pixels.height())) {
                continue;
            }
            if (pixels.at(point + dir) != 1) {
                continue;
            }
            numNeighboorsSelf++;
            glm::ivec2 neighboor = point + dir;
            int numNeighboorsNeighboor{0};
            for (const auto& nDir: DIRECTIONS) {
                if (!isInside(neighboor, nDir, pixels.width(), pixels.height())) {
                    continue;
                }
                if (pixels.at(neighboor + nDir) != 1) {
                    continue;
                }
                numNeighboorsNeighboor++;
            }
            if (numNeighboorsNeighboor > 1) {
                leafs.erase(point + dir);
            }
        }

        if (numNeighboorsSelf <= 1) {
            leafs.insert(point);
        }

        return true;
    }

    float DLABasic::noise(std::size_t x, std::size_t y) const {
        return 0.0F;
    }


    // DLA with dual filter blur
    DLADualFilterBlur::DLADualFilterBlur(std::size_t numSteps, HeightFunc heightFunc)
        : DLADualFilterBlur{numSteps, std::random_device{}(), heightFunc} {}
    DLADualFilterBlur::DLADualFilterBlur(std::size_t numSteps, std::uint32_t seed, HeightFunc heightFunc)
        : DLABasic{numSteps, seed, heightFunc} {
        setSeed(seed);
    }

    /*
    So essantially for each pixel remember its relative coordinate to grid and remember its neighboor
    */
    void DLADualFilterBlur::getRelativeCoordinates(HeightMap<int>& pixelsOld, glm::ivec2 startingPos,
            std::vector<std::pair<glm::vec2, glm::vec2>>& relCoordinates) const {

        std::unordered_set<std::pair<glm::ivec2, glm::ivec2>, Vec2Vec2Hash> counted{};
        std::queue<std::pair<glm::ivec2, glm::ivec2>> q{};
        for (const auto& dir : DIRECTIONS) {
            if (!isInside(startingPos, dir, pixelsOld.width(), pixelsOld.height())) {
                continue;
            }
            if (pixelsOld.at(startingPos + dir) != 1) {
                continue;
            }
            glm::ivec2 neighboor = startingPos + dir;
            q.push({startingPos, neighboor});
            counted.insert({startingPos, neighboor});counted.insert({neighboor, startingPos});
        }

        while (q.size() > 0) {
            std::pair<glm::ivec2, glm::ivec2> sf = q.front();
            q.pop();
            glm::ivec2 start = sf.first;
            glm::ivec2 end = sf.second;

            glm::vec2 startRel = {
                static_cast<float>(start.x) / static_cast<float>(pixelsOld.width() - 1),
                static_cast<float>(start.y) / static_cast<float>(pixelsOld.height() - 1)
            };
            glm::vec2 endRel = {
                static_cast<float>(end.x) / static_cast<float>(pixelsOld.width() - 1),
                static_cast<float>(end.y) / static_cast<float>(pixelsOld.height() - 1)
            };
            relCoordinates.push_back({startRel, endRel});

            for (const auto& dir : DIRECTIONS) {
                if (!isInside(end, dir, pixelsOld.width(), pixelsOld.height())) {
                    continue;
                }
                if (pixelsOld.at(end + dir) != 1
                    || counted.contains({end, end + dir}) || counted.contains({end + dir, end})) {
                    continue;
                }
                glm::ivec2 neighboor = end + dir;
                q.push({end, neighboor});
                counted.insert({end, neighboor});counted.insert({neighboor, end});
            }
        }
    }

    void DLADualFilterBlur::connectTwoPoints(
            HeightMap<int>& pixels,
            glm::ivec2 p1,
            glm::ivec2 p2) const {
        const int deltaX = std::abs(p2.x - p1.x);
        const int stepX = p1.x < p2.x ? 1 : -1;
        const int deltaY = -std::abs(p2.y - p1.y);
        const int stepY = p1.y < p2.y ? 1 : -1;
        int error = deltaX + deltaY;

        while (true) {
            pixels.at(p1) = 1;
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

    void DLADualFilterBlur::constructUpscaledPixels(std::vector<std::pair<glm::vec2, glm::vec2>>& relCoordinates, HeightMap<int>& pixels) const {
        pixels.clear(0);
        for (const auto& relPair : relCoordinates) {
            glm::vec2 startRel = relPair.first;
            glm::vec2 endRel = relPair.second;
            glm::ivec2 start = {
                startRel.x * static_cast<float>(pixels.width() - 1),
                startRel.y * static_cast<float>(pixels.height() - 1),
            };
            glm::ivec2 end = {
                endRel.x * static_cast<float>(pixels.width() - 1),
                endRel.y * static_cast<float>(pixels.height() - 1),
            };
            connectTwoPoints(pixels, start, end);
        }
    }

}
