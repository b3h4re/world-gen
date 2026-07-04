#include "gabor.hpp"

#include "terrain/utils/hash_random.hpp"

#include <glm/gtc/constants.hpp>

namespace wgen {

GaborNoise::GaborNoise(std::size_t dotsPerCell, SeedType seed, float impulseDensity,
            float kernelSpatialExtent, float kernelOscillationFrequency)
            : dotsPerCell_{dotsPerCell}, impulseDensity_{impulseDensity}, kernelSpatialExtent_{kernelSpatialExtent},
            kernelOscillationFrequency_{kernelOscillationFrequency} {
    Generator::setSeed(seed);
}


float GaborNoise::noise(std::size_t x, std::size_t y) const {
        const std::size_t gridX = x / dotsPerCell_;
        const std::size_t gridY = y / dotsPerCell_;
        const glm::vec2 point{
            static_cast<float>(x) / static_cast<float>(dotsPerCell_),
            static_cast<float>(y) / static_cast<float>(dotsPerCell_)
        };
        SeedType seed = getSeed();
        HashUniformRealDistribution<float> random{0.0F, 1.0F};

        const std::size_t effectivekernelRadius = static_cast<std::size_t>(
            std::ceil(3.0F / static_cast<float>(kernelSpatialExtent_))
        );
        const std::size_t leftBound = effectivekernelRadius > gridX ? 0 : gridX - effectivekernelRadius;
        const std::size_t rightBound = gridX + effectivekernelRadius;

        const std::size_t borromBound = effectivekernelRadius > gridY ? 0 : gridY - effectivekernelRadius;
        const std::size_t topBound = gridY + effectivekernelRadius;

        float noise = 0.0F;

        for (std::size_t p = leftBound; p <= rightBound; ++p) {
            for (std::size_t q = borromBound; q <= topBound; ++q) {
                std::uint64_t cellSeed = hashValues(seed, p, q);
                int M = poisson(impulseDensity_, cellSeed);
                for (int i = 0; i < M; ++i) {
                    float xi = random(hashValues(cellSeed, i, 0));
                    float eta = random(hashValues(cellSeed, i, 1));

                    float weight = toUnitFloat(hashValues(cellSeed, i, 2));
                    float angle  = 2.0f * glm::pi<float>() * random(hashValues(cellSeed, i, 3));
                    float phase  = 2.0f * glm::pi<float>() * random(hashValues(cellSeed, i, 4));


                    glm::vec2 impulsePos{
                        static_cast<float>(p) + xi,
                        static_cast<float>(q) + eta
                    };
                    glm::vec2 r = point - impulsePos;
                    glm::vec2 d{
                        glm::cos(angle),
                        glm::sin(angle)
                    };

                    noise += weight * glm::exp(- glm::pi<float>() * kernelSpatialExtent_*kernelSpatialExtent_ * glm::dot(r, r)) *
                                glm::cos(glm::two_pi<float>() * kernelOscillationFrequency_ * glm::dot(d, r) + phase);
                }


            }
        }

        return noise;
    }

    GeneratorCapabilities GaborNoise::capabilities() const {
        return {
            .cpu = true,
            .vulkanCompute = false,
            .octaves = false,
        };
    }

    std::string GaborNoise::compShader() const {
        return "gabor_noise";
    }

    std::size_t GaborNoise::specSize() const {
        return sizeof(GaborNoiseComputeSpec);
    }


}
