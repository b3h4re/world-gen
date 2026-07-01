#include "dla.hpp"


#include <unordered_map>
#include <queue>
#include <iostream>
#include <vector>


namespace wgen {

    RandomGridPoints::RandomGridPoints() : RandomGridPoints{std::random_device{}()} {}
    RandomGridPoints::RandomGridPoints(SeedType seed) : seed_{seed} {
        this->reset();
    }

    void RandomGridPoints::reset() {
        random_ = std::mt19937(seed_);
    }

    void RandomGridPoints::setSeed(SeedType newSeed) {
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
    DLABasic::DLABasic(std::size_t numSteps, SeedType seed, HeightFunc heightFunc)
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
        std::unordered_set<glm::ivec2, Ivec2Hash> placedPoints{};
        placedPoints.insert(startingPos);
        std::unordered_set<glm::ivec2, Ivec2Hash> leafs{};
        leafs.insert(startingPos);
        for (std::size_t i = 0; i < numSteps_; ++i) {
            if (!dlaStep(pixels, points, random, placedPoints, leafs)) {
                break;
            }
        }
        // findLeafs(pixels, leafs);
        auto dists = enumPixelsFromLeafs(pixels, leafs);


        for (const auto& p : placedPoints) {
            res.at(p) = heightFunc_(dists.at(p));
        }

        return res;
    }

    HeightMap<int> DLABasic::enumPixelsFromSource(const HeightMap<int>& pixels, glm::ivec2 startingPos, std::unordered_set<glm::ivec2, Ivec2Hash>& placedPoints, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs) {
        std::queue<glm::ivec2> q{};
        HeightMap<int> dist(pixels.width(), pixels.height(), 0);

        q.push(startingPos);
        dist.at(startingPos) = 1;

        enumPoints(pixels, dist, q);

        return dist;
    }


    HeightMap<int> DLABasic::enumPixelsFromLeafs(const HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs) {
        std::queue<glm::ivec2> q{};
        HeightMap<int> dist(pixels.width(), pixels.height(), 0);

        for (const auto& leaf : leafs) {
            q.push(leaf);
            dist.at(leaf) = 1;
        }

        enumPoints(pixels, dist, q);

        return dist;
    }

    void enumPoints(const HeightMap<int>& pixels, HeightMap<int>& dist, std::queue<glm::ivec2> q) {
        while (q.size() > 0) {
            glm::ivec2 pos = q.front();q.pop();

            for (const auto& dir : DIRECTIONS) {
                if (!isInside(pos, dir, pixels.width(), pixels.height())) {
                    continue;
                }
                glm::ivec2 nextPos = pos + dir;
                if (pixels.at(nextPos) == 0 || dist.at(nextPos) != 0) {
                    continue;
                }
                dist.at(nextPos)= dist.at(pos) + 1;
                q.push(nextPos);
            }
        }
    }

    glm::ivec2 DLABasic::getRandomDirection(std::mt19937& randomDevice) {
        return DIRECTIONS[randomDevice() % 4];
    }

    bool DLABasic::isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) {
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
            std::unordered_set<glm::ivec2, Ivec2Hash>& placedPoints,
            std::unordered_set<glm::ivec2, Ivec2Hash>& leafs)
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

        if (numNeighboorsSelf == 1) {
            leafs.insert(point);
        }

        if (numNeighboorsSelf == 0) {
            std::string message = "DLA step placed a disconnected pixel at position (";
            message += std::to_string(point.x);
            message += ", "; message += std::to_string(point.y); message += ")\n";
            throw std::runtime_error(message);
        }

        return true;
    }

    float DLABasic::noise(std::size_t x, std::size_t y) const {
        return 0.0F;
    }


    // DLA with dual filter blur
    DLADualFilterBlur::DLADualFilterBlur(std::size_t numSteps, HeightFunc heightFunc, float fill, float jiggle)
        : DLADualFilterBlur{numSteps, std::random_device{}(), heightFunc, fill, jiggle} {}
    DLADualFilterBlur::DLADualFilterBlur(std::size_t numSteps, SeedType seed, HeightFunc heightFunc, float fill, float jiggle)
        : DLABasic{numSteps, seed, heightFunc}, fill_{fill}, jiggle_{jiggle} {
        setSeed(seed);
    }

    /*
    So essantially for each pixel remember its relative coordinate to grid and remember its neighboor
    */
    PointGraph<glm::vec2, Vec2Hash> DLADualFilterBlur::getRelativeCoordinates(HeightMap<int>& pixels) {
        glm::ivec2 startingPos;
        for (std::size_t x = 0; x < pixels.width(); ++x) {
            for (std::size_t y = 0; y < pixels.height(); ++y) {
                if (pixels.at(x, y) == 1) {
                    startingPos = glm::ivec2{x, y};
                    return getRelativeCoordinates(pixels, startingPos);
                }
            }
        }
        return PointGraph<glm::vec2, Vec2Hash>{};
    }
    PointGraph<glm::vec2, Vec2Hash> DLADualFilterBlur::getRelativeCoordinates(HeightMap<int>& pixels, glm::ivec2 startingPos) {
        auto leafs = getLeafs(pixels);
        return getRelativeCoordinates(pixels, startingPos, leafs);
    }

    PointGraph<glm::vec2, Vec2Hash> DLADualFilterBlur::getRelativeCoordinates(HeightMap<int>& pixels, glm::ivec2 startingPos, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs) {
        assert(pixels.at(startingPos) == 1 && "Starting position must be a placed pixel");
        // A vector where at v[i] is a list of neighboors of i
        PointGraph<glm::ivec2, Ivec2Hash> intGraph{};
        // std::vector<std::unordered_set<std::size_t>> pointGraph{};
        // std::unordered_map<glm::ivec2, int, Vec2Hash> posIndicies{}; // m[v] is index of a vertex with position v
        // std::vector<glm::ivec2> vertexPositions{}; // v[i] is position of vertex i in pixel grid

        std::unordered_set<std::pair<glm::ivec2, glm::ivec2>, Vec2Vec2Hash> visited{}; // travelled edges

        std::queue<std::pair<glm::ivec2, glm::ivec2>> q{};

        intGraph.pointGraph.emplace_back();
        intGraph.posIndicies.insert({startingPos, 0});
        // intGraph.posIndicies.at(startingPos) = 0;
        intGraph.vertexPositions.push_back(startingPos);

        q.push({startingPos, startingPos});


        while (q.size() > 0) {
            std::pair<glm::ivec2, glm::ivec2> sf = q.front(); q.pop();
            glm::ivec2 prev = sf.first;
            glm::ivec2 cur = sf.second;

            if (!intGraph.posIndicies.contains(cur)) {
                intGraph.pointGraph.emplace_back();
                intGraph.posIndicies.insert({cur, intGraph.pointGraph.size() - 1});
                intGraph.vertexPositions.push_back(cur);
            }

            int curId = intGraph.posIndicies[cur];
            int prevId = intGraph.posIndicies[prev];
            intGraph.pointGraph[curId].insert(prevId);
            intGraph.pointGraph[prevId].insert(curId);


            for (const auto& dir : DIRECTIONS) {
                if (!isInside(cur, dir, pixels.width(), pixels.height())) {
                    continue;
                }
                if (pixels.at(cur + dir) != 1) {
                    continue;
                }
                glm::ivec2 neighboor = cur + dir;
                if (visited.contains({cur, neighboor}) || visited.contains({neighboor, cur})) {
                    continue;
                }

                q.push({cur, neighboor});
                visited.insert({cur, neighboor});
                visited.insert({neighboor, cur});
            }
        }

        // Now construct graph with relative positions
        // pointGraph is the same but convert positions from int grid to float vals
        PointGraph<glm::vec2, Vec2Hash> relPointGraph{};
        for (std::size_t i = 0; i < intGraph.pointGraph.size(); ++i) {
            glm::ivec2 pos = intGraph.vertexPositions[i];
            glm::vec2 relPos = {
                static_cast<float>(pos.x) / static_cast<float>(pixels.width() - 1),
                static_cast<float>(pos.y) / static_cast<float>(pixels.height() - 1)
            };
            relPointGraph.vertexPositions.push_back(relPos);
            relPointGraph.posIndicies.insert({relPos, i});
        }


        for (const auto& leaf : leafs) {
            relPointGraph.leafs.insert(intGraph.posIndicies.at(leaf));
        }
        relPointGraph.pointGraph = std::move(intGraph.pointGraph);
        return relPointGraph;
    }

    // Just do bfs. Get each vertex position on new grid and just connect them
    std::unordered_set<glm::ivec2, Ivec2Hash> DLADualFilterBlur::constructCrispUpscaledPixels(const PointGraph<glm::vec2, Vec2Hash>& relPointGraph, HeightMap<int>& pixels) {
        std::queue<std::pair<std::size_t, std::size_t>> q{};
        std::unordered_set<std::pair<std::size_t, std::size_t>, SizeSizeHash> travelled{};

        q.push({0, 0});

        while (q.size() > 0) {
            std::pair<std::size_t, std::size_t> sf = q.front(); q.pop();
            std::size_t prevId = sf.first;
            std::size_t curId = sf.second;
            glm::vec2 prevRel = relPointGraph.vertexPositions[prevId];
            glm::vec2 curRel = relPointGraph.vertexPositions[curId];

            glm::ivec2 prev = {
                prevRel.x * static_cast<float>(pixels.width() - 1),
                prevRel.y * static_cast<float>(pixels.height() - 1),
            };
            glm::ivec2 cur = {
                curRel.x * static_cast<float>(pixels.width() - 1),
                curRel.y * static_cast<float>(pixels.height() - 1),
            };

            connectTwoPoints(pixels, prev, cur);
            for (const auto& neighboorId : relPointGraph.pointGraph[curId]) {
                if (travelled.contains({curId, neighboorId}) || travelled.contains({neighboorId, curId})) {
                    continue;
                }
                q.push({curId, neighboorId});
                travelled.insert({curId, neighboorId});
                travelled.insert({neighboorId, curId});
            }
        }
        std::unordered_set<glm::ivec2, Ivec2Hash> newLeafs{};
        for (const auto& relLeaf : relPointGraph.leafs) {
            auto relLeafPos = relPointGraph.vertexPositions[relLeaf];
            glm::ivec2 leafPos = {
                relLeafPos.x * static_cast<float>(pixels.width() - 1),
                relLeafPos.y * static_cast<float>(pixels.height() - 1),
            };
            newLeafs.insert(leafPos);
        }
        return newLeafs;
    }

    HeightMap<float> DLADualFilterBlur::heightMapFromPixels(const HeightMap<int>& pixels) const {
        HeightMap<float> res{pixels.width(), pixels.height()};
        HeightMap<int> pixelsTmp{pixels};
        auto leafs = getLeafs(pixels);
        std::unordered_set<glm::ivec2, Ivec2Hash> placedPoints{};
        for (std::size_t x = 0; x < pixels.width(); ++x) {
            for (std::size_t y = 0; y < pixels.height(); ++y) {
                if (pixels.at(x, y) == 1) {
                    placedPoints.insert(glm::ivec2{x, y});
                }
            }
        }

        enumPixelsFromLeafs(pixelsTmp, leafs);
        for (const auto& p : placedPoints) {
            res.at(p) = heightFunc_(pixelsTmp.at(p));
        }

        return res;
    }


    HeightMap<float> DLADualFilterBlur::heightMapFromPointGraph(const PointGraph<glm::vec2, Vec2Hash>& praph, std::size_t width, std::size_t height) const {
        HeightMap<int> pixels{width, height};
        constructCrispUpscaledPixels(praph, pixels);
        return heightMapFromPixels(pixels);
    }

    std::unordered_set<glm::ivec2, Ivec2Hash> DLADualFilterBlur::getLeafs(const HeightMap<int>& pixels) {
        std::unordered_set<glm::ivec2, Ivec2Hash> leafs{};

        for (std::size_t y = 0; y < pixels.height(); ++y) {
            for (std::size_t x = 0; x < pixels.width(); ++x) {
                if (pixels.at(x, y) == 0) {
                    continue;
                }

                glm::ivec2 pos{
                    static_cast<int>(x),
                    static_cast<int>(y)
                };
                int numNeighboors{0};

                for (const auto& dir : DIRECTIONS) {
                    if (!isInside(pos, dir, pixels.width(), pixels.height())) {
                        continue;
                    }
                    if (pixels.at(pos + dir) != 0) {
                        ++numNeighboors;
                    }
                }

                if (numNeighboors <= 1) {
                    leafs.insert(pos);
                }
            }
        }

        return leafs;
    }

    void DLADualFilterBlur::fillPixels(HeightMap<int>& pixels, std::unordered_set<glm::ivec2, Ivec2Hash>& leafs, std::mt19937& randomDevice) const {
        std::unordered_set<glm::ivec2, Ivec2Hash> placedPoints{};
        for (std::size_t x = 0; x < pixels.width(); ++x) {
            for (std::size_t y = 0; y < pixels.height(); ++y) {
                if (pixels.at(x, y) == 1) {
                    placedPoints.insert(glm::ivec2{x, y});
                }
            }
        }
        std::size_t targetSize = static_cast<std::size_t>(
            fill_ * static_cast<float>(pixels.width() * pixels.height())
        );
        SeedType innerSeed = randomDevice();
        RandomGridPoints points{innerSeed};

        while (placedPoints.size() < targetSize) {
            if (!dlaStep(pixels, points, randomDevice, placedPoints, leafs)) {
                break;
            }
        }
    }

    void DLADualFilterBlur::jigglePoints(PointGraph<glm::vec2, Vec2Hash>& pgraph, float sigma, std::mt19937& rd) {
        std::normal_distribution<float> normalDist(0, sigma);
        for (std::size_t vertex = 0; vertex < pgraph.pointGraph.size(); ++vertex) {
            glm::vec2 oldPos = pgraph.vertexPositions[vertex];
            glm::vec2 offset{
                normalDist(rd),
                normalDist(rd)
            };
            glm::vec2 newPos = oldPos + offset;
            newPos.x = std::min(1.0F, newPos.x);
            newPos.x = std::max(0.0F, newPos.x);
            newPos.y = std::min(1.0F, newPos.y);
            newPos.y = std::max(0.0F, newPos.y);

            pgraph.vertexPositions[vertex] = newPos;
            pgraph.posIndicies.erase(oldPos);
            pgraph.posIndicies.emplace(newPos, vertex);
        }
    }

    HeightMap<float> DLADualFilterBlur::generateHeightMap(std::size_t widthTarget, std::size_t heightTarget) const {
        std::size_t deltaW = widthTarget / (numSteps_ + 1);
        std::size_t deltaH = heightTarget / (numSteps_ + 1);
        assert(
            deltaW > 0 && deltaH > 0 && "Target image too small to generate with numSteps"
        );
        // Generate a starting small image
        std::size_t width = deltaW, height = deltaH;
        HeightMap<int> pixelsStart{width, height};
        glm::ivec2 startingPos{width/2, height/2};
        pixelsStart.at(startingPos) = 1;
        RandomGridPoints points{getSeed()};

        std::mt19937 random{getSeed()};
        std::unordered_set<glm::ivec2, Ivec2Hash> placedPoints{};
        placedPoints.insert(startingPos);
        std::unordered_set<glm::ivec2, Ivec2Hash> leafs{};
        leafs.insert(startingPos);
        std::size_t pointToPlace = static_cast<std::size_t>(fill_ * static_cast<float>(width * height));
        for (std::size_t i = 0; i < pointToPlace; ++i) {
            if (!dlaStep(pixelsStart, points, random, placedPoints, leafs)) {
                break;
            }
        }
        PointGraph<glm::vec2, Vec2Hash> graph = getRelativeCoordinates(pixelsStart, startingPos, leafs);



        HeightMap<float> initialHeightMap = heightMapFromPixels(pixelsStart);


        for (const auto& p : placedPoints) {
            initialHeightMap.at(p) = heightFunc_(pixelsStart.at(p));
        }

        for (std::size_t step = 1; step < numSteps_ + 1; ++step) {
            std::size_t newWidth = width + step * deltaW;
            std::size_t newHeight = height + step * deltaH;
            if (step == numSteps_) {
                newWidth = widthTarget;
                newHeight = heightTarget;
            }

            // Making a crisp upscale and its heightmap
            HeightMap<int> crispUpscale{newWidth, newHeight};
            auto crispLeafs = constructCrispUpscaledPixels(graph, crispUpscale);
            fillPixels(crispUpscale, crispLeafs, random);
            HeightMap<float> crispHeightMap = heightMapFromPixels(crispUpscale);


            HeightMap<float> upscaledInitialHeightMap{newWidth, newHeight};
            upscaleHeightmapWeightedLerp(initialHeightMap, upscaledInitialHeightMap);
            upscaledInitialHeightMap = conv(upscaledInitialHeightMap, wgen::SMALL_BLUR);

            upscaledInitialHeightMap += crispHeightMap;

            initialHeightMap = upscaledInitialHeightMap;
            pixelsStart = crispUpscale;
            graph = getRelativeCoordinates(pixelsStart);
            jigglePoints(graph, jiggle_, random);
        }
        return initialHeightMap;
    }

}
