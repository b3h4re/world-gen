#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace wgen {

inline constexpr std::uint32_t DEFAULT_LOCAL_CLIPMAP_SAMPLES_PER_AXIS = 65;
inline constexpr std::uint32_t DEFAULT_LOCAL_CLIPMAP_LEVEL_COUNT = 7;
inline constexpr double DEFAULT_LOCAL_CLIPMAP_FINEST_CELL_SPACING_METERS = 1.0;
inline constexpr std::uint32_t DEFAULT_LOCAL_CLIPMAP_LEVEL_SCALE = 2;
inline constexpr std::uint32_t DEFAULT_LOCAL_CLIPMAP_OVERLAP_WIDTH_CELLS = 2;
inline constexpr std::uint32_t DEFAULT_LOCAL_CLIPMAP_TRANSITION_WIDTH_CELLS = 2;

// Widths are measured in cells of the coarser level. Level zero is always the
// finest local level. The transition band is the inner part of the overlap and
// may later be morphed or masked independently from the rest of a ring.
struct LocalClipmapConfig {
    std::uint32_t samplesPerAxis{DEFAULT_LOCAL_CLIPMAP_SAMPLES_PER_AXIS};
    std::uint32_t levelCount{DEFAULT_LOCAL_CLIPMAP_LEVEL_COUNT};
    double finestCellSpacingMeters{DEFAULT_LOCAL_CLIPMAP_FINEST_CELL_SPACING_METERS};
    std::uint32_t levelScale{DEFAULT_LOCAL_CLIPMAP_LEVEL_SCALE};
    std::uint32_t overlapWidthCells{DEFAULT_LOCAL_CLIPMAP_OVERLAP_WIDTH_CELLS};
    std::uint32_t transitionWidthCells{DEFAULT_LOCAL_CLIPMAP_TRANSITION_WIDTH_CELLS};
};

enum class LocalClipmapPatternKind : std::uint8_t {
    Center,
    Ring,
};

enum class LocalClipmapTopologyPart : std::uint8_t {
    Center,
    RingBody,
    TrimWest,
    TrimEast,
    TrimSouth,
    TrimNorth,
    FixupSouthWest,
    FixupSouthEast,
    FixupNorthWest,
    FixupNorthEast,
};

struct LocalClipmapGridVertex {
    std::int32_t x{};
    std::int32_t y{};

    bool operator==(const LocalClipmapGridVertex&) const = default;
};

struct LocalClipmapDrawRange {
    LocalClipmapTopologyPart part{};
    std::uint32_t firstIndex{};
    std::uint32_t indexCount{};

    bool operator==(const LocalClipmapDrawRange&) const = default;
};

// Both patterns index the same position-independent grid vertex array. Every
// coarser level therefore shares one immutable ring pattern.
struct LocalClipmapIndexPattern {
    LocalClipmapPatternKind kind{};
    std::vector<std::uint32_t> indices{};
    std::vector<LocalClipmapDrawRange> ranges{};
};

struct LocalClipmapLevelLayout {
    std::uint32_t level{};
    double cellSpacingMeters{};
    LocalClipmapPatternKind pattern{};
};

struct LocalClipmapTopology {
    LocalClipmapConfig config{};
    std::uint32_t outerHalfExtentCells{};
    std::uint32_t ringInnerHalfExtentCells{};
    std::uint32_t transitionOuterHalfExtentCells{};
    std::vector<LocalClipmapGridVertex> vertices{};
    LocalClipmapIndexPattern centerPattern{};
    LocalClipmapIndexPattern ringPattern{};
    std::vector<LocalClipmapLevelLayout> levels{};
};

struct LocalClipmapGridCoordinate {
    std::int64_t x{};
    std::int64_t y{};

    bool operator==(const LocalClipmapGridCoordinate&) const = default;
};

// Cache coordinates remain integral even when the tangent frame is re-anchored.
// frameRevision prevents coordinates from different local frames being reused.
struct LocalClipmapCacheOrigin {
    glm::dvec2 localFrameAnchorMeters{};
    std::uint64_t frameRevision{};
    std::uint32_t level{};
    LocalClipmapGridCoordinate centerSample{};
};

enum class LocalClipmapUpdateKind : std::uint8_t {
    Full,
    Columns,
    Rows,
};

// Half-open sample rectangle [minimum, minimum + size).
struct LocalClipmapUpdateRegion {
    LocalClipmapUpdateKind kind{};
    LocalClipmapGridCoordinate minimum{};
    std::uint32_t widthSamples{};
    std::uint32_t heightSamples{};

    bool operator==(const LocalClipmapUpdateRegion&) const = default;
};

struct LocalClipmapUpdateSet {
    bool fullRefresh{};
    std::vector<LocalClipmapUpdateRegion> regions{};
};

void validateLocalClipmapConfig(const LocalClipmapConfig& config);
double localClipmapCellSpacing(const LocalClipmapConfig& config, std::uint32_t level);
LocalClipmapTopology buildLocalClipmapTopology(const LocalClipmapConfig& config);

LocalClipmapCacheOrigin snapLocalClipmapOrigin(
    const LocalClipmapConfig& config,
    std::uint32_t level,
    const glm::dvec2& localFrameAnchorMeters,
    std::uint64_t frameRevision,
    const glm::dvec2& cameraPositionMeters);

LocalClipmapGridCoordinate localClipmapWindowMinimum(
    const LocalClipmapConfig& config,
    const LocalClipmapCacheOrigin& origin);

glm::dvec2 localClipmapSamplePosition(
    const LocalClipmapCacheOrigin& origin,
    const LocalClipmapGridCoordinate& sample,
    double cellSpacingMeters);

glm::dvec2 localClipmapVertexPosition(
    const LocalClipmapCacheOrigin& origin,
    const LocalClipmapGridVertex& vertex,
    double cellSpacingMeters);

// Returns disjoint rectangles. Columns are emitted first; rows exclude those
// columns, so a diagonal one-cell move updates exactly 2N - 1 samples.
LocalClipmapUpdateSet localClipmapExposedRegions(
    const LocalClipmapConfig& config,
    const LocalClipmapCacheOrigin& previous,
    const LocalClipmapCacheOrigin& current);

} // namespace wgen
