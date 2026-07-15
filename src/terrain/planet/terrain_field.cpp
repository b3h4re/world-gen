#include "terrain_field.hpp"

#include "terrain/generators/3d/generator_factory.hpp"
#include "terrain/utils/hash_random.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace wgen {

namespace {

bool isFinite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

float TerrainHeightCalibration::apply(float rawHeight) const {
    if (!std::isfinite(rawHeight)) {
        throw std::invalid_argument{"terrain field raw height must be finite"};
    }

    const float result = rawHeight * scale + bias;
    if (!std::isfinite(result)) {
        throw std::overflow_error{"terrain field calibrated height is not finite"};
    }
    return result;
}

TerrainField::TerrainField(Generator3dPipelineSpec pipeline, SeedType seed, float radius)
    : radius_{radius}, detailPolicy_{radius} {
    if (!std::isfinite(radius_) || radius_ <= 0.0F) {
        throw std::invalid_argument{"terrain field radius must be finite and positive"};
    }

    contributors_.reserve(pipeline.size());
    SeedType generatorSeed = seed;
    for (const Generator3dSpec& spec : pipeline) {
        const std::uint8_t firstFullyVisibleDetail =
            generator3dFirstFullyVisibleDetail(spec);
        if (firstFullyVisibleDetail > MAX_TERRAIN_DETAIL_LEVEL) {
            throw std::invalid_argument{"generator terrain detail exceeds the supported maximum"};
        }

        const float amplitude = spec.scale * generator3dOctaveAmplitude(spec);
        if (!std::isfinite(amplitude)) {
            throw std::invalid_argument{"terrain field generator amplitude must be finite"};
        }

        contributors_.push_back({
            makePipelineGenerator3d(spec, generatorSeed),
            amplitude,
            terrainDetailBandForFirstFullyVisibleLevel(firstFullyVisibleDetail),
        });
        maximumAbsoluteRawHeight_ +=
            std::abs(amplitude) * generator3dMaximumAbsoluteNoise(spec.kind);
        if (!std::isfinite(maximumAbsoluteRawHeight_)) {
            throw std::overflow_error{"terrain field height bound is not finite"};
        }
        generatorSeed = hashSeed(generatorSeed);
    }

    calibrate();
    maximumAbsoluteHeight_ = std::max(
        std::abs(calibration_.apply(-maximumAbsoluteRawHeight_)),
        std::abs(calibration_.apply(maximumAbsoluteRawHeight_)));
}

float TerrainField::sample(
        const PlanetSurfaceSample& surface,
        TerrainDetailLevel detail) const {
    static_cast<void>(faceID(surface.face));

    const glm::vec3 direction{surface.direction};
    if (!isFinite(direction)) {
        throw std::invalid_argument{"terrain field sample direction must be finite"};
    }

    return calibration_.apply(sampleRaw(direction, detail));
}

float TerrainField::sample(
        const PlanetSurfaceSample& surface,
        std::uint8_t maxDetailLevel) const {
    return sample(surface, TerrainDetailLevel::fromDiscrete(maxDetailLevel));
}

CubeSphere<float> TerrainField::generateCubeSphere(
        std::size_t resolution,
        TerrainDetailLevel detail) const {
    if (resolution < 2) {
        throw std::invalid_argument{"terrain field cube sphere resolution must be at least two"};
    }

    CubeSphere<float> result{radius_, resolution, 0.0F};
    for (const CubeSphereFace face : FACES) {
        for (std::size_t y = 0; y < resolution; ++y) {
            for (std::size_t x = 0; x < resolution; ++x) {
                const float denominator = static_cast<float>(resolution - 1);
                const float u = -1.0F + 2.0F * static_cast<float>(x) / denominator;
                const float v = -1.0F + 2.0F * static_cast<float>(y) / denominator;
                const glm::vec3 direction = result.pointUnitDir(face, x, y);
                result.at(face, x, y) = sample(
                    {face, static_cast<double>(u), static_cast<double>(v), glm::dvec3{direction}},
                    detail);
            }
        }
    }
    return result;
}

CubeSphere<float> TerrainField::generateCubeSphere(
        std::size_t resolution,
        std::uint8_t maxDetailLevel) const {
    return generateCubeSphere(
        resolution,
        TerrainDetailLevel::fromDiscrete(maxDetailLevel));
}

float TerrainField::sampleRaw(glm::vec3 direction, TerrainDetailLevel detail) const {
    float result = 0.0F;
    for (const Contributor& contributor : contributors_) {
        const float detailWeight = terrainDetailBandWeight(detail, contributor.detailBand);
        if (detailWeight == 0.0F) {
            continue;
        }

        result += contributor.generator->noise(direction) * contributor.amplitude * detailWeight;
        if (!std::isfinite(result)) {
            throw std::invalid_argument{"terrain field generator produced a non-finite height"};
        }
    }
    return result;
}

void TerrainField::calibrate() {
    if (contributors_.empty()) {
        calibration_ = {};
        return;
    }

    float rawMinimum = std::numeric_limits<float>::infinity();
    float rawMaximum = -std::numeric_limits<float>::infinity();
    const float denominator = static_cast<float>(TERRAIN_CALIBRATION_RESOLUTION - 1);
    const TerrainDetailLevel maximumDetail =
        TerrainDetailLevel::fromDiscrete(MAX_TERRAIN_DETAIL_LEVEL);
    for (const CubeSphereFace face : FACES) {
        for (std::size_t y = 0; y < TERRAIN_CALIBRATION_RESOLUTION; ++y) {
            for (std::size_t x = 0; x < TERRAIN_CALIBRATION_RESOLUTION; ++x) {
                const float u = -1.0F + 2.0F * static_cast<float>(x) / denominator;
                const float v = -1.0F + 2.0F * static_cast<float>(y) / denominator;
                const float rawHeight = sampleRaw(
                    cubeSphereDirection(face, u, v),
                    maximumDetail);
                rawMinimum = std::min(rawMinimum, rawHeight);
                rawMaximum = std::max(rawMaximum, rawHeight);
            }
        }
    }

    calibration_.rawMinimum = rawMinimum;
    calibration_.rawMaximum = rawMaximum;
    if (std::abs(rawMaximum - rawMinimum) < 0.000001F) {
        calibration_.scale = 0.0F;
        calibration_.bias = 0.0F;
        return;
    }

    calibration_.scale = 2.0F / (rawMaximum - rawMinimum);
    calibration_.bias = -1.0F - rawMinimum * calibration_.scale;
}

TerrainFieldSnapshot buildTerrainFieldSnapshot(
        const Generator3dPipelineSpec& pipeline,
        SeedType seed,
        float radius) {
    return std::make_shared<const TerrainField>(pipeline, seed, radius);
}

} // namespace wgen
