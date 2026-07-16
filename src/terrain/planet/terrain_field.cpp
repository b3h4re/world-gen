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

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

float TerrainField::HeightTransform::apply(float authoredHeight) const {
    if (!std::isfinite(authoredHeight)) {
        throw std::invalid_argument{"terrain field authored height must be finite"};
    }

    const float result = authoredHeight * scale + bias;
    if (!std::isfinite(result)) {
        throw std::overflow_error{"terrain field physical height is not finite"};
    }
    return result;
}

TerrainField::TerrainField(
        Generator3dPipelineSpec pipeline,
        SeedType seed,
        float radiusMeters,
        TerrainHeightSemantics heightSemantics)
    : radiusMeters_{radiusMeters},
      heightSemantics_{heightSemantics},
      detailPolicy_{radiusMeters} {
    if (!std::isfinite(radiusMeters_) || radiusMeters_ <= 0.0F) {
        throw std::invalid_argument{"terrain field radius must be finite and positive"};
    }
    if (heightSemantics_ != TerrainHeightSemantics::PhysicalMeters &&
            heightSemantics_ != TerrainHeightSemantics::LegacyNormalized) {
        throw std::invalid_argument{"terrain field height semantics are invalid"};
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
        if (!std::isfinite(amplitude) || !std::isfinite(spec.bias)) {
            throw std::invalid_argument{"terrain field generator scale and bias must be finite"};
        }
        const float maximumAbsoluteNoiseHeight =
            std::abs(amplitude) * generator3dMaximumAbsoluteNoise(spec.kind);
        const float maximumAbsoluteAuthoredHeight =
            maximumAbsoluteNoiseHeight + std::abs(spec.bias);
        if (!std::isfinite(maximumAbsoluteAuthoredHeight)) {
            throw std::overflow_error{"terrain field contributor height bound is not finite"};
        }

        contributors_.push_back({
            makePipelineGenerator3d(spec, generatorSeed),
            amplitude,
            spec.bias,
            maximumAbsoluteNoiseHeight,
            maximumAbsoluteAuthoredHeight,
            terrainDetailBandForFirstFullyVisibleLevel(firstFullyVisibleDetail),
        });
        generatorSeed = hashSeed(generatorSeed);
    }

    if (heightSemantics_ == TerrainHeightSemantics::LegacyNormalized) {
        calibrateLegacyHeightTransform();
    }
    buildHeightMetadata();
}

float TerrainField::sample(
        glm::dvec3 direction,
        TerrainDetailLevel detail) const {
    if (!isFinite(direction)) {
        throw std::invalid_argument{"terrain field sample direction must be finite"};
    }

    return geometryHeightTransform_.apply(sampleAuthored(
        direction,
        detail));
}

float TerrainField::sample(
        const PlanetSurfaceSample& surface,
        TerrainDetailLevel detail) const {
    static_cast<void>(faceID(surface.face));
    return sample(surface.direction, detail);
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

    CubeSphere<float> result{radiusMeters_, resolution, 0.0F};
    for (const CubeSphereFace face : FACES) {
        for (std::size_t y = 0; y < resolution; ++y) {
            for (std::size_t x = 0; x < resolution; ++x) {
                const double denominator = static_cast<double>(resolution - 1);
                const double u = -1.0 + 2.0 * static_cast<double>(x) / denominator;
                const double v = -1.0 + 2.0 * static_cast<double>(y) / denominator;
                const glm::dvec3 direction = result.pointUnitDirection(face, x, y);
                result.at(face, x, y) = sample(
                    {face, u, v, direction},
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

float TerrainField::sampleAuthored(
        glm::dvec3 direction,
        TerrainDetailLevel detail) const {
    float result = 0.0F;
    for (const Contributor& contributor : contributors_) {
        const float detailWeight = terrainDetailBandWeight(detail, contributor.detailBand);
        if (detailWeight == 0.0F) {
            continue;
        }

        result += (contributor.generator->noise(direction) * contributor.amplitude +
            contributor.bias) * detailWeight;
        if (!std::isfinite(result)) {
            throw std::invalid_argument{"terrain field generator produced a non-finite height"};
        }
    }
    return result;
}

void TerrainField::calibrateLegacyHeightTransform() {
    if (contributors_.empty()) {
        geometryHeightTransform_ = {.scale = 0.0F, .bias = 0.0F};
        return;
    }

    float rawMinimum = std::numeric_limits<float>::infinity();
    float rawMaximum = -std::numeric_limits<float>::infinity();
    const double denominator = static_cast<double>(
        LEGACY_TERRAIN_CALIBRATION_RESOLUTION - 1);
    const TerrainDetailLevel maximumDetail =
        TerrainDetailLevel::fromDiscrete(MAX_TERRAIN_DETAIL_LEVEL);
    for (const CubeSphereFace face : FACES) {
        for (std::size_t y = 0; y < LEGACY_TERRAIN_CALIBRATION_RESOLUTION; ++y) {
            for (std::size_t x = 0; x < LEGACY_TERRAIN_CALIBRATION_RESOLUTION; ++x) {
                const double u = -1.0 + 2.0 * static_cast<double>(x) / denominator;
                const double v = -1.0 + 2.0 * static_cast<double>(y) / denominator;
                const float authoredHeight = sampleAuthored(
                    cubeSphereDirection(face, u, v),
                    maximumDetail);
                rawMinimum = std::min(rawMinimum, authoredHeight);
                rawMaximum = std::max(rawMaximum, authoredHeight);
            }
        }
    }

    if (std::abs(rawMaximum - rawMinimum) < 0.000001F) {
        geometryHeightTransform_ = {.scale = 0.0F, .bias = 0.0F};
        return;
    }

    geometryHeightTransform_.scale = 2.0F / (rawMaximum - rawMinimum);
    geometryHeightTransform_.bias = -1.0F - rawMinimum * geometryHeightTransform_.scale;
}

void TerrainField::buildHeightMetadata() {
    float minimumAuthoredHeight = 0.0F;
    float maximumAuthoredHeight = 0.0F;
    for (const Contributor& contributor : contributors_) {
        minimumAuthoredHeight += std::min(
            0.0F,
            contributor.bias - contributor.maximumAbsoluteNoiseHeight);
        maximumAuthoredHeight += std::max(
            0.0F,
            contributor.bias + contributor.maximumAbsoluteNoiseHeight);
    }

    const float transformedMinimum = geometryHeightTransform_.apply(minimumAuthoredHeight);
    const float transformedMaximum = geometryHeightTransform_.apply(maximumAuthoredHeight);
    heightBounds_.minimumDisplacementMeters =
        std::min(transformedMinimum, transformedMaximum);
    heightBounds_.maximumDisplacementMeters =
        std::max(transformedMinimum, transformedMaximum);
    heightBounds_.maximumAbsoluteDisplacementMeters = std::max(
        std::abs(heightBounds_.minimumDisplacementMeters),
        std::abs(heightBounds_.maximumDisplacementMeters));

    for (std::uint8_t level = 0; level <= MAX_TERRAIN_DETAIL_LEVEL; ++level) {
        const TerrainDetailLevel detail = TerrainDetailLevel::fromDiscrete(level);
        float authoredError = 0.0F;
        for (const Contributor& contributor : contributors_) {
            const float visibleWeight = terrainDetailBandWeight(detail, contributor.detailBand);
            authoredError += contributor.maximumAbsoluteAuthoredHeight * (1.0F - visibleWeight);
        }
        const float physicalError = std::abs(geometryHeightTransform_.scale) * authoredError;
        if (!std::isfinite(physicalError)) {
            throw std::overflow_error{"terrain field omitted-detail bound is not finite"};
        }
        heightBounds_.maximumOmittedDetailErrorMeters[level] = physicalError;
    }

    if (heightSemantics_ == TerrainHeightSemantics::LegacyNormalized) {
        displayHeightRange_ = {.minimumMeters = -1.0F, .maximumMeters = 1.0F};
    } else if (heightBounds_.maximumDisplacementMeters -
            heightBounds_.minimumDisplacementMeters < 0.000001F) {
        displayHeightRange_ = {.minimumMeters = -1.0F, .maximumMeters = 1.0F};
    } else {
        displayHeightRange_ = {
            .minimumMeters = heightBounds_.minimumDisplacementMeters,
            .maximumMeters = heightBounds_.maximumDisplacementMeters,
        };
    }
}

TerrainFieldSnapshot buildTerrainFieldSnapshot(
        const Generator3dPipelineSpec& pipeline,
        SeedType seed,
        float radiusMeters,
        TerrainHeightSemantics heightSemantics) {
    return std::make_shared<const TerrainField>(
        pipeline,
        seed,
        radiusMeters,
        heightSemantics);
}

} // namespace wgen
