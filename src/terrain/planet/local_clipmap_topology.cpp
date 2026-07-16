#include "terrain/planet/local_clipmap_topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace wgen {
namespace {

constexpr std::array<LocalClipmapTopologyPart, 9> RING_PARTS{
    LocalClipmapTopologyPart::RingBody,
    LocalClipmapTopologyPart::TrimWest,
    LocalClipmapTopologyPart::TrimEast,
    LocalClipmapTopologyPart::TrimSouth,
    LocalClipmapTopologyPart::TrimNorth,
    LocalClipmapTopologyPart::FixupSouthWest,
    LocalClipmapTopologyPart::FixupSouthEast,
    LocalClipmapTopologyPart::FixupNorthWest,
    LocalClipmapTopologyPart::FixupNorthEast,
};

bool isFinite(const glm::dvec2& value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

std::int64_t checkedAdd(std::int64_t value, std::int64_t offset) {
    if ((offset > 0 && value > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && value < std::numeric_limits<std::int64_t>::min() - offset)) {
        throw std::overflow_error{"local clipmap grid coordinate overflow"};
    }
    return value + offset;
}

std::int64_t snappedCoordinate(double relativePosition, double spacing) {
    const double snapped = std::floor(relativePosition / spacing);
    if (!std::isfinite(snapped) ||
        snapped < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        snapped >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error{"local clipmap snapped coordinate is outside int64 range"};
    }
    return static_cast<std::int64_t>(snapped);
}

std::uint32_t vertexIndex(
    std::int32_t x,
    std::int32_t y,
    std::uint32_t samplesPerAxis,
    std::int32_t halfExtent) {
    const std::uint32_t column = static_cast<std::uint32_t>(x + halfExtent);
    const std::uint32_t row = static_cast<std::uint32_t>(y + halfExtent);
    return row * samplesPerAxis + column;
}

void appendCell(
    std::vector<std::uint32_t>& indices,
    std::int32_t x,
    std::int32_t y,
    std::uint32_t samplesPerAxis,
    std::int32_t halfExtent) {
    const std::uint32_t southWest = vertexIndex(x, y, samplesPerAxis, halfExtent);
    const std::uint32_t southEast = vertexIndex(x + 1, y, samplesPerAxis, halfExtent);
    const std::uint32_t northWest = vertexIndex(x, y + 1, samplesPerAxis, halfExtent);
    const std::uint32_t northEast = vertexIndex(x + 1, y + 1, samplesPerAxis, halfExtent);

    indices.insert(indices.end(), {
        southWest, southEast, northEast,
        southWest, northEast, northWest,
    });
}

bool cellInsideSquare(std::int32_t x, std::int32_t y, std::int32_t halfExtent) {
    return x >= -halfExtent && x < halfExtent &&
        y >= -halfExtent && y < halfExtent;
}

LocalClipmapTopologyPart classifyRingCell(
    std::int32_t x,
    std::int32_t y,
    std::int32_t innerHalfExtent,
    std::int32_t transitionOuterHalfExtent) {
    if (!cellInsideSquare(x, y, transitionOuterHalfExtent)) {
        return LocalClipmapTopologyPart::RingBody;
    }

    const bool westOrEast = x < -innerHalfExtent || x >= innerHalfExtent;
    const bool southOrNorth = y < -innerHalfExtent || y >= innerHalfExtent;
    if (westOrEast && southOrNorth) {
        if (y < 0) {
            return x < 0
                ? LocalClipmapTopologyPart::FixupSouthWest
                : LocalClipmapTopologyPart::FixupSouthEast;
        }
        return x < 0
            ? LocalClipmapTopologyPart::FixupNorthWest
            : LocalClipmapTopologyPart::FixupNorthEast;
    }
    if (westOrEast) {
        return x < 0
            ? LocalClipmapTopologyPart::TrimWest
            : LocalClipmapTopologyPart::TrimEast;
    }
    return y < 0
        ? LocalClipmapTopologyPart::TrimSouth
        : LocalClipmapTopologyPart::TrimNorth;
}

std::size_t partIndex(LocalClipmapTopologyPart part) {
    const auto found = std::find(RING_PARTS.begin(), RING_PARTS.end(), part);
    if (found == RING_PARTS.end()) {
        throw std::logic_error{"invalid local clipmap ring part"};
    }
    return static_cast<std::size_t>(std::distance(RING_PARTS.begin(), found));
}

LocalClipmapUpdateSet fullRefresh(
    const LocalClipmapConfig& config,
    const LocalClipmapCacheOrigin& current) {
    const LocalClipmapGridCoordinate minimum = localClipmapWindowMinimum(config, current);
    return {
        true,
        {{
            LocalClipmapUpdateKind::Full,
            minimum,
            config.samplesPerAxis,
            config.samplesPerAxis,
        }},
    };
}

std::uint32_t rectangleSize(std::int64_t minimum, std::int64_t maximum) {
    if (maximum < minimum) {
        throw std::logic_error{"invalid local clipmap update rectangle"};
    }
    const auto size = static_cast<std::uint64_t>(maximum - minimum);
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{"local clipmap update rectangle is too large"};
    }
    return static_cast<std::uint32_t>(size);
}

} // namespace

void validateLocalClipmapConfig(const LocalClipmapConfig& config) {
    if (config.samplesPerAxis < 9 || config.samplesPerAxis % 2 == 0) {
        throw std::invalid_argument{"local clipmap samples per axis must be odd and at least 9"};
    }
    if (config.levelCount == 0) {
        throw std::invalid_argument{"local clipmap must contain at least one level"};
    }
    if (!std::isfinite(config.finestCellSpacingMeters) ||
        config.finestCellSpacingMeters <= 0.0) {
        throw std::invalid_argument{"local clipmap finest cell spacing must be finite and positive"};
    }
    if (config.levelScale != 2) {
        throw std::invalid_argument{"local clipmap level scale must initially be 2"};
    }

    const std::uint32_t halfExtent = (config.samplesPerAxis - 1) / 2;
    if (halfExtent % config.levelScale != 0) {
        throw std::invalid_argument{"local clipmap half extent must be divisible by the level scale"};
    }
    const std::uint32_t fineCoverageHalfExtent = halfExtent / config.levelScale;
    if (config.overlapWidthCells == 0 ||
        config.overlapWidthCells >= fineCoverageHalfExtent) {
        throw std::invalid_argument{"local clipmap overlap must leave a non-empty ring hole"};
    }
    if (config.transitionWidthCells == 0 ||
        config.transitionWidthCells > config.overlapWidthCells) {
        throw std::invalid_argument{"local clipmap transition width must be in the overlap"};
    }

    const std::uint64_t sampleCount =
        static_cast<std::uint64_t>(config.samplesPerAxis) * config.samplesPerAxis;
    const std::uint64_t maximumIndexCount =
        static_cast<std::uint64_t>(config.samplesPerAxis - 1) *
        (config.samplesPerAxis - 1) * 6;
    if (sampleCount > std::numeric_limits<std::uint32_t>::max() ||
        maximumIndexCount > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument{"local clipmap grid is too large for 32-bit indices"};
    }

    double spacing = config.finestCellSpacingMeters;
    for (std::uint32_t level = 1; level < config.levelCount; ++level) {
        spacing *= static_cast<double>(config.levelScale);
        if (!std::isfinite(spacing)) {
            throw std::invalid_argument{"local clipmap level spacing overflows double precision"};
        }
    }
}

double localClipmapCellSpacing(const LocalClipmapConfig& config, std::uint32_t level) {
    validateLocalClipmapConfig(config);
    if (level >= config.levelCount) {
        throw std::out_of_range{"local clipmap level is outside the configured range"};
    }

    double spacing = config.finestCellSpacingMeters;
    for (std::uint32_t current = 0; current < level; ++current) {
        spacing *= static_cast<double>(config.levelScale);
    }
    return spacing;
}

LocalClipmapTopology buildLocalClipmapTopology(const LocalClipmapConfig& config) {
    validateLocalClipmapConfig(config);

    LocalClipmapTopology topology{};
    topology.config = config;
    topology.outerHalfExtentCells = (config.samplesPerAxis - 1) / 2;
    const std::uint32_t fineCoverageHalfExtent =
        topology.outerHalfExtentCells / config.levelScale;
    topology.ringInnerHalfExtentCells =
        fineCoverageHalfExtent - config.overlapWidthCells;
    topology.transitionOuterHalfExtentCells =
        topology.ringInnerHalfExtentCells + config.transitionWidthCells;

    const auto halfExtent = static_cast<std::int32_t>(topology.outerHalfExtentCells);
    topology.vertices.reserve(
        static_cast<std::size_t>(config.samplesPerAxis) * config.samplesPerAxis);
    for (std::int32_t y = -halfExtent; y <= halfExtent; ++y) {
        for (std::int32_t x = -halfExtent; x <= halfExtent; ++x) {
            topology.vertices.push_back({x, y});
        }
    }

    topology.centerPattern.kind = LocalClipmapPatternKind::Center;
    topology.centerPattern.indices.reserve(
        static_cast<std::size_t>(config.samplesPerAxis - 1) *
        (config.samplesPerAxis - 1) * 6);
    for (std::int32_t y = -halfExtent; y < halfExtent; ++y) {
        for (std::int32_t x = -halfExtent; x < halfExtent; ++x) {
            appendCell(
                topology.centerPattern.indices,
                x,
                y,
                config.samplesPerAxis,
                halfExtent);
        }
    }
    topology.centerPattern.ranges.push_back({
        LocalClipmapTopologyPart::Center,
        0,
        static_cast<std::uint32_t>(topology.centerPattern.indices.size()),
    });

    topology.ringPattern.kind = LocalClipmapPatternKind::Ring;
    std::array<std::vector<std::uint32_t>, RING_PARTS.size()> partIndices{};
    const auto innerHalfExtent =
        static_cast<std::int32_t>(topology.ringInnerHalfExtentCells);
    const auto transitionOuterHalfExtent =
        static_cast<std::int32_t>(topology.transitionOuterHalfExtentCells);
    for (std::int32_t y = -halfExtent; y < halfExtent; ++y) {
        for (std::int32_t x = -halfExtent; x < halfExtent; ++x) {
            if (cellInsideSquare(x, y, innerHalfExtent)) {
                continue;
            }
            const LocalClipmapTopologyPart part = classifyRingCell(
                x,
                y,
                innerHalfExtent,
                transitionOuterHalfExtent);
            appendCell(
                partIndices[partIndex(part)],
                x,
                y,
                config.samplesPerAxis,
                halfExtent);
        }
    }

    for (std::size_t index = 0; index < RING_PARTS.size(); ++index) {
        const std::uint32_t firstIndex =
            static_cast<std::uint32_t>(topology.ringPattern.indices.size());
        topology.ringPattern.indices.insert(
            topology.ringPattern.indices.end(),
            partIndices[index].begin(),
            partIndices[index].end());
        topology.ringPattern.ranges.push_back({
            RING_PARTS[index],
            firstIndex,
            static_cast<std::uint32_t>(partIndices[index].size()),
        });
    }

    topology.levels.reserve(config.levelCount);
    double spacing = config.finestCellSpacingMeters;
    for (std::uint32_t level = 0; level < config.levelCount; ++level) {
        topology.levels.push_back({
            level,
            spacing,
            level == 0
                ? LocalClipmapPatternKind::Center
                : LocalClipmapPatternKind::Ring,
        });
        spacing *= static_cast<double>(config.levelScale);
    }

    return topology;
}

LocalClipmapCacheOrigin snapLocalClipmapOrigin(
    const LocalClipmapConfig& config,
    std::uint32_t level,
    const glm::dvec2& localFrameAnchorMeters,
    std::uint64_t frameRevision,
    const glm::dvec2& cameraPositionMeters) {
    if (!isFinite(localFrameAnchorMeters) || !isFinite(cameraPositionMeters)) {
        throw std::invalid_argument{"local clipmap anchor and camera position must be finite"};
    }
    const double spacing = localClipmapCellSpacing(config, level);
    const glm::dvec2 relativePosition = cameraPositionMeters - localFrameAnchorMeters;
    if (!isFinite(relativePosition)) {
        throw std::overflow_error{"local clipmap relative camera position overflow"};
    }

    return {
        localFrameAnchorMeters,
        frameRevision,
        level,
        {
            snappedCoordinate(relativePosition.x, spacing),
            snappedCoordinate(relativePosition.y, spacing),
        },
    };
}

LocalClipmapGridCoordinate localClipmapWindowMinimum(
    const LocalClipmapConfig& config,
    const LocalClipmapCacheOrigin& origin) {
    validateLocalClipmapConfig(config);
    if (origin.level >= config.levelCount) {
        throw std::out_of_range{"local clipmap cache origin level is outside the configured range"};
    }
    const auto halfExtent = static_cast<std::int64_t>((config.samplesPerAxis - 1) / 2);
    return {
        checkedAdd(origin.centerSample.x, -halfExtent),
        checkedAdd(origin.centerSample.y, -halfExtent),
    };
}

glm::dvec2 localClipmapSamplePosition(
    const LocalClipmapCacheOrigin& origin,
    const LocalClipmapGridCoordinate& sample,
    double cellSpacingMeters) {
    if (!isFinite(origin.localFrameAnchorMeters) ||
        !std::isfinite(cellSpacingMeters) || cellSpacingMeters <= 0.0) {
        throw std::invalid_argument{"local clipmap sample transform is invalid"};
    }
    const glm::dvec2 offset{
        static_cast<double>(sample.x) * cellSpacingMeters,
        static_cast<double>(sample.y) * cellSpacingMeters,
    };
    const glm::dvec2 position = origin.localFrameAnchorMeters + offset;
    if (!isFinite(position)) {
        throw std::overflow_error{"local clipmap sample position overflow"};
    }
    return position;
}

glm::dvec2 localClipmapVertexPosition(
    const LocalClipmapCacheOrigin& origin,
    const LocalClipmapGridVertex& vertex,
    double cellSpacingMeters) {
    const LocalClipmapGridCoordinate sample{
        checkedAdd(origin.centerSample.x, vertex.x),
        checkedAdd(origin.centerSample.y, vertex.y),
    };
    return localClipmapSamplePosition(origin, sample, cellSpacingMeters);
}

LocalClipmapUpdateSet localClipmapExposedRegions(
    const LocalClipmapConfig& config,
    const LocalClipmapCacheOrigin& previous,
    const LocalClipmapCacheOrigin& current) {
    validateLocalClipmapConfig(config);
    if (previous.level >= config.levelCount || current.level >= config.levelCount) {
        throw std::out_of_range{"local clipmap update origin level is outside the configured range"};
    }

    if (previous.level != current.level ||
        previous.frameRevision != current.frameRevision ||
        previous.localFrameAnchorMeters != current.localFrameAnchorMeters) {
        return fullRefresh(config, current);
    }

    const LocalClipmapGridCoordinate oldMinimum =
        localClipmapWindowMinimum(config, previous);
    const LocalClipmapGridCoordinate newMinimum =
        localClipmapWindowMinimum(config, current);
    const auto sampleCount = static_cast<std::int64_t>(config.samplesPerAxis);
    const LocalClipmapGridCoordinate oldMaximum{
        checkedAdd(oldMinimum.x, sampleCount),
        checkedAdd(oldMinimum.y, sampleCount),
    };
    const LocalClipmapGridCoordinate newMaximum{
        checkedAdd(newMinimum.x, sampleCount),
        checkedAdd(newMinimum.y, sampleCount),
    };

    const std::int64_t retainedMinimumX = std::max(oldMinimum.x, newMinimum.x);
    const std::int64_t retainedMaximumX = std::min(oldMaximum.x, newMaximum.x);
    const std::int64_t retainedMinimumY = std::max(oldMinimum.y, newMinimum.y);
    const std::int64_t retainedMaximumY = std::min(oldMaximum.y, newMaximum.y);
    if (retainedMinimumX >= retainedMaximumX || retainedMinimumY >= retainedMaximumY) {
        return fullRefresh(config, current);
    }

    LocalClipmapUpdateSet updates{};
    if (newMinimum.x < oldMinimum.x) {
        updates.regions.push_back({
            LocalClipmapUpdateKind::Columns,
            newMinimum,
            rectangleSize(newMinimum.x, oldMinimum.x),
            config.samplesPerAxis,
        });
    } else if (newMaximum.x > oldMaximum.x) {
        updates.regions.push_back({
            LocalClipmapUpdateKind::Columns,
            {oldMaximum.x, newMinimum.y},
            rectangleSize(oldMaximum.x, newMaximum.x),
            config.samplesPerAxis,
        });
    }

    const std::uint32_t retainedWidth =
        rectangleSize(retainedMinimumX, retainedMaximumX);
    if (newMinimum.y < oldMinimum.y) {
        updates.regions.push_back({
            LocalClipmapUpdateKind::Rows,
            {retainedMinimumX, newMinimum.y},
            retainedWidth,
            rectangleSize(newMinimum.y, oldMinimum.y),
        });
    } else if (newMaximum.y > oldMaximum.y) {
        updates.regions.push_back({
            LocalClipmapUpdateKind::Rows,
            {retainedMinimumX, oldMaximum.y},
            retainedWidth,
            rectangleSize(oldMaximum.y, newMaximum.y),
        });
    }

    return updates;
}

} // namespace wgen
