#include "dla.hpp"


namespace wgen {

    RandomGridPoints::RandomGridPoints(std::size_t width, std::size_t height)
            : RandomGridPoints{width, height, std::random_device{}()} {}
    RandomGridPoints::RandomGridPoints(std::size_t width, std::size_t height, std::uint32_t seed)
            : seed_{seed}, width_{width}, height_{height} {
        this->clear();
    }

    void RandomGridPoints::clear() {
        excluded_points.clear();
        this->reset();
    }
    void RandomGridPoints::reset() {
        random_ = std::mt19937(seed_);
    }

    void RandomGridPoints::setSeed(std::uint32_t newSeed) {
        seed_ = newSeed;
        this->reset();
    }

    RandomGridPoints::Point RandomGridPoints::next() {
        std::uniform_int_distribution<int> x_dist(0, static_cast<int>(width_ - 1));
        std::uniform_int_distribution<int> y_dist(0, static_cast<int>(height_ - 1));
        while (true) {
            Point p{x_dist(random_), y_dist(random_)};

            if (excluded_points.contains(p)) {
                continue;
            }
            return p;
        }
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
        RandomGridPoints points{gridWidth_, gridHeight_, getSeed()};
        HeightMap<float> res{width, height};

        std::mt19937 random{getSeed()};
        for (std::size_t i = 0; i < numSteps_; ++i) {
            dlaStep(pixels, points, random);
        }

        return res;
    }

    glm::ivec2 DLABasic::getRandomDirection(std::mt19937 randomDevice) {
        return DIRECTIONS[randomDevice() % 4];
    }

    bool DLABasic::isAdjacent(HeightMap<int>& pixels, glm::ivec2 point) const {
        for (int i = 0; i < 4; ++i) {
            glm::ivec2 adjacentPosition = point + DIRECTIONS[i];
            if (adjacentPosition.x > gridWidth_ || adjacentPosition.y > gridHeight_
                || adjacentPosition.x < 0 || adjacentPosition.y < 0) {
                continue;
            }
            if (pixels.at(adjacentPosition) == 1) {
                return true;
            }
        }
        return false;
    }

    void DLABasic::dlaStep(HeightMap<int>& pixels, RandomGridPoints& points, std::mt19937 randomDevice) const {
        glm::ivec2 point = points.next();

        while (!isAdjacent(pixels, point)) {
            glm::ivec2 dir = getRandomDirection(randomDevice);
            glm::ivec2 nextPos = point + dir;
            if (nextPos.x > gridWidth_ || nextPos.y > gridHeight_ || nextPos.x < 0 || nextPos.y < 0) {
                continue;
            }
            point = nextPos;
        }
        points.exclude(point);
        pixels.at(point) = 1;
    }

}
