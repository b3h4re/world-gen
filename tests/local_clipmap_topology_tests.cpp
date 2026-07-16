#include "terrain/planet/local_clipmap_topology.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

using Cell = std::pair<std::int64_t, std::int64_t>;

const wgen::LocalClipmapIndexPattern& patternForLevel(
        const wgen::LocalClipmapTopology& topology,
        const wgen::LocalClipmapLevelLayout& level) {
    return level.pattern == wgen::LocalClipmapPatternKind::Center
        ? topology.centerPattern
        : topology.ringPattern;
}

void requirePatternValid(
        const wgen::LocalClipmapTopology& topology,
        const wgen::LocalClipmapIndexPattern& pattern) {
    wgen::tests::require(
        pattern.indices.size() % 6 == 0,
        "clipmap cells should contain two triangles");
    for (const std::uint32_t index : pattern.indices) {
        wgen::tests::require(
            index < topology.vertices.size(),
            "clipmap index is out of bounds");
    }
    for (std::size_t index = 0; index < pattern.indices.size(); index += 3) {
        const wgen::LocalClipmapGridVertex a =
            topology.vertices[pattern.indices[index]];
        const wgen::LocalClipmapGridVertex b =
            topology.vertices[pattern.indices[index + 1]];
        const wgen::LocalClipmapGridVertex c =
            topology.vertices[pattern.indices[index + 2]];
        const std::int64_t signedDoubleArea =
            static_cast<std::int64_t>(b.x - a.x) * (c.y - a.y) -
            static_cast<std::int64_t>(b.y - a.y) * (c.x - a.x);
        wgen::tests::require(
            signedDoubleArea > 0,
            "clipmap triangle winding should be counter-clockwise");
    }

    std::uint32_t expectedFirstIndex = 0;
    for (const wgen::LocalClipmapDrawRange& range : pattern.ranges) {
        wgen::tests::require(
            range.firstIndex == expectedFirstIndex,
            "clipmap draw ranges should be contiguous");
        wgen::tests::require(
            range.indexCount > 0 && range.indexCount % 6 == 0,
            "clipmap draw range should contain complete cells");
        expectedFirstIndex += range.indexCount;
    }
    wgen::tests::require(
        expectedFirstIndex == pattern.indices.size(),
        "clipmap draw ranges should cover the complete pattern");
}

std::set<Cell> patternCells(
        const wgen::LocalClipmapTopology& topology,
        const wgen::LocalClipmapIndexPattern& pattern) {
    std::set<Cell> cells;
    for (std::size_t index = 0; index < pattern.indices.size(); index += 6) {
        const wgen::LocalClipmapGridVertex southWest =
            topology.vertices[pattern.indices[index]];
        const bool inserted = cells.emplace(southWest.x, southWest.y).second;
        wgen::tests::require(inserted, "clipmap pattern contains a duplicate cell");
    }
    return cells;
}

void testConfigAndSharedPatterns() {
    const wgen::LocalClipmapConfig config{};
    wgen::validateLocalClipmapConfig(config);
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);

    wgen::tests::require(
        topology.levels.size() == config.levelCount,
        "clipmap topology should describe every configured level");
    wgen::tests::require(
        topology.levels.front().level == 0 &&
            topology.levels.front().pattern ==
                wgen::LocalClipmapPatternKind::Center,
        "clipmap level zero should be the filled finest level");
    for (std::uint32_t level = 1; level < config.levelCount; ++level) {
        wgen::tests::require(
            topology.levels[level].level == level &&
                topology.levels[level].pattern ==
                    wgen::LocalClipmapPatternKind::Ring,
            "coarser clipmap levels should reuse the ring pattern");
        wgen::tests::require(
            topology.levels[level].cellSpacingMeters ==
                config.finestCellSpacingMeters *
                    static_cast<double>(std::uint64_t{1} << level),
            "clipmap cell spacing should double at every level");
    }
    wgen::tests::require(
        topology.vertices.size() ==
            static_cast<std::size_t>(config.samplesPerAxis) *
                config.samplesPerAxis,
        "center and ring patterns should share one vertex lattice");
    wgen::tests::require(
        topology.ringPattern.ranges.size() == 9,
        "ring topology should expose a body, four trims, and four fix-ups");
}

void testValidation() {
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 64;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmaps should reject an even sample count");

    config = {};
    config.levelCount = 0;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmaps should reject an empty level set");

    config = {};
    config.levelScale = 3;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmaps should reject unsupported level scales");

    config = {};
    config.overlapWidthCells = 0;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmaps should require overlap between levels");

    config = {};
    config.transitionWidthCells = config.overlapWidthCells + 1;
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmap transition should fit inside overlap");

    config = {};
    config.finestCellSpacingMeters =
        std::numeric_limits<double>::infinity();
    wgen::tests::requireThrows<std::invalid_argument>(
        [&config] { wgen::validateLocalClipmapConfig(config); },
        "clipmaps should reject non-finite spacing");
}

void testIndexBoundsWindingAndPartition() {
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology({});
    requirePatternValid(topology, topology.centerPattern);
    requirePatternValid(topology, topology.ringPattern);

    const std::set<Cell> centerCells =
        patternCells(topology, topology.centerPattern);
    const std::set<Cell> ringCells =
        patternCells(topology, topology.ringPattern);
    const std::uint64_t quadsPerAxis = topology.config.samplesPerAxis - 1;
    const std::uint64_t holeWidth =
        2 * topology.ringInnerHalfExtentCells;
    wgen::tests::require(
        centerCells.size() == quadsPerAxis * quadsPerAxis,
        "filled center should contain every grid cell");
    wgen::tests::require(
        ringCells.size() ==
            quadsPerAxis * quadsPerAxis - holeWidth * holeWidth,
        "ring parts should partition the grid outside the inner hole");
}

void testCompleteMultilevelCoverage() {
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 17;
    config.levelCount = 4;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);

    std::set<Cell> coveredFineCells;
    std::int64_t scale = 1;
    for (const wgen::LocalClipmapLevelLayout& level : topology.levels) {
        const std::set<Cell> cells = patternCells(
            topology,
            patternForLevel(topology, level));
        for (const Cell& cell : cells) {
            for (std::int64_t y = cell.second * scale;
                 y < (cell.second + 1) * scale;
                 ++y) {
                for (std::int64_t x = cell.first * scale;
                     x < (cell.first + 1) * scale;
                     ++x) {
                    coveredFineCells.emplace(x, y);
                }
            }
        }
        scale *= config.levelScale;
    }

    const std::int64_t coarsestScale = scale / config.levelScale;
    const std::int64_t outer =
        static_cast<std::int64_t>(topology.outerHalfExtentCells) *
        coarsestScale;
    for (std::int64_t y = -outer; y < outer; ++y) {
        for (std::int64_t x = -outer; x < outer; ++x) {
            wgen::tests::require(
                coveredFineCells.contains({x, y}),
                "multilevel clipmap should leave no planar coverage hole");
        }
    }
}

void testFineAndCoarseBoundariesCoincide() {
    wgen::LocalClipmapConfig config{};
    config.levelCount = 2;
    const wgen::LocalClipmapTopology topology =
        wgen::buildLocalClipmapTopology(config);
    const glm::dvec2 anchor{1'000.125, -2'000.75};
    const glm::dvec2 camera = anchor + glm::dvec2{123.4, -45.6};
    const wgen::LocalClipmapCacheOrigin fine =
        wgen::snapLocalClipmapOrigin(config, 0, anchor, 9, camera);
    const wgen::LocalClipmapCacheOrigin coarse =
        wgen::snapLocalClipmapOrigin(config, 1, anchor, 9, camera);
    const wgen::LocalClipmapGridCoordinate fineMinimum =
        wgen::localClipmapWindowMinimum(config, fine);
    const std::int64_t fineMaximumX =
        fineMinimum.x + config.samplesPerAxis - 1;
    const std::int64_t fineMaximumY =
        fineMinimum.y + config.samplesPerAxis - 1;

    const auto inner =
        static_cast<std::int32_t>(topology.ringInnerHalfExtentCells);
    std::set<std::pair<std::int32_t, std::int32_t>> boundaryVertices;
    for (const std::uint32_t index : topology.ringPattern.indices) {
        const wgen::LocalClipmapGridVertex vertex = topology.vertices[index];
        const bool onVerticalBoundary =
            std::abs(vertex.x) == inner &&
            vertex.y >= -inner && vertex.y <= inner;
        const bool onHorizontalBoundary =
            std::abs(vertex.y) == inner &&
            vertex.x >= -inner && vertex.x <= inner;
        if (onVerticalBoundary || onHorizontalBoundary) {
            boundaryVertices.emplace(vertex.x, vertex.y);
        }
    }

    wgen::tests::require(
        !boundaryVertices.empty(),
        "ring topology should expose an inner handoff boundary");
    const double coarseSpacing = wgen::localClipmapCellSpacing(config, 1);
    for (const auto& [x, y] : boundaryVertices) {
        const glm::dvec2 position = wgen::localClipmapVertexPosition(
            coarse,
            {x, y},
            coarseSpacing);
        const glm::dvec2 fineGrid =
            (position - fine.localFrameAnchorMeters) /
            config.finestCellSpacingMeters;
        const auto fineX = static_cast<std::int64_t>(std::llround(fineGrid.x));
        const auto fineY = static_cast<std::int64_t>(std::llround(fineGrid.y));
        wgen::tests::require(
            std::abs(fineGrid.x - static_cast<double>(fineX)) <= 0.000000001 &&
                std::abs(fineGrid.y - static_cast<double>(fineY)) <= 0.000000001,
            "coarse handoff vertices should lie on the fine sample lattice");
        wgen::tests::require(
            fineX >= fineMinimum.x && fineX <= fineMaximumX &&
                fineY >= fineMinimum.y && fineY <= fineMaximumY,
            "coarse handoff boundary should remain inside fine coverage");
    }
}

void testStableSnappedOrigins() {
    wgen::LocalClipmapConfig config{};
    config.levelCount = 3;
    const glm::dvec2 anchor{1'000.125, -2'000.75};
    const wgen::LocalClipmapCacheOrigin first =
        wgen::snapLocalClipmapOrigin(
            config,
            1,
            anchor,
            17,
            anchor + glm::dvec2{5.9, -0.1});
    const wgen::LocalClipmapCacheOrigin sameCell =
        wgen::snapLocalClipmapOrigin(
            config,
            1,
            anchor,
            17,
            anchor + glm::dvec2{5.99, -0.001});
    const wgen::LocalClipmapCacheOrigin nextCell =
        wgen::snapLocalClipmapOrigin(
            config,
            1,
            anchor,
            17,
            anchor + glm::dvec2{6.01, 0.0});

    wgen::tests::require(
        first.centerSample == wgen::LocalClipmapGridCoordinate{2, -1} &&
            sameCell.centerSample == first.centerSample,
        "snapped origins should remain stable inside a cell");
    wgen::tests::require(
        nextCell.centerSample == wgen::LocalClipmapGridCoordinate{3, 0},
        "snapped origins should advance exactly at cell boundaries");
    wgen::tests::require(
        first.localFrameAnchorMeters == anchor && first.frameRevision == 17,
        "cache coordinates should retain their double anchor and frame revision");
    const glm::dvec2 position = wgen::localClipmapSamplePosition(
        first,
        first.centerSample,
        wgen::localClipmapCellSpacing(config, 1));
    wgen::tests::require(
        glm::length(position - (anchor + glm::dvec2{4.0, -2.0})) <=
            0.000000001,
        "integer cache coordinates should reconstruct their snapped position");
}

std::uint32_t updateSampleCount(const wgen::LocalClipmapUpdateSet& updates) {
    std::uint32_t count = 0;
    for (const wgen::LocalClipmapUpdateRegion& region : updates.regions) {
        count += region.widthSamples * region.heightSamples;
    }
    return count;
}

wgen::LocalClipmapCacheOrigin makeOrigin(
        std::int64_t x,
        std::int64_t y,
        glm::dvec2 anchor = {},
        std::uint64_t revision = 1) {
    return {anchor, revision, 0, {x, y}};
}

void testMinimalExposedRegions() {
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 9;
    config.levelCount = 2;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const wgen::LocalClipmapCacheOrigin origin = makeOrigin(0, 0);

    const wgen::LocalClipmapUpdateSet still =
        wgen::localClipmapExposedRegions(config, origin, origin);
    wgen::tests::require(
        !still.fullRefresh && still.regions.empty(),
        "stationary clipmap should not expose cache samples");

    const wgen::LocalClipmapUpdateSet oneColumn =
        wgen::localClipmapExposedRegions(config, origin, makeOrigin(1, 0));
    wgen::tests::require(
        !oneColumn.fullRefresh && oneColumn.regions.size() == 1 &&
            oneColumn.regions.front() == wgen::LocalClipmapUpdateRegion{
                wgen::LocalClipmapUpdateKind::Columns,
                {5, -4},
                1,
                9,
            },
        "one-cell movement should expose exactly one cache column");

    const wgen::LocalClipmapUpdateSet diagonal =
        wgen::localClipmapExposedRegions(config, origin, makeOrigin(1, 1));
    wgen::tests::require(
        !diagonal.fullRefresh && diagonal.regions.size() == 2 &&
            updateSampleCount(diagonal) == 17,
        "diagonal one-cell movement should update two disjoint strips");
    wgen::tests::require(
        diagonal.regions[0] == wgen::LocalClipmapUpdateRegion{
            wgen::LocalClipmapUpdateKind::Columns,
            {5, -3},
            1,
            9,
        } &&
            diagonal.regions[1] == wgen::LocalClipmapUpdateRegion{
                wgen::LocalClipmapUpdateKind::Rows,
                {-3, 5},
                8,
                1,
            },
        "diagonal cache strips should be deterministic and non-overlapping");

    const wgen::LocalClipmapUpdateSet negative =
        wgen::localClipmapExposedRegions(config, origin, makeOrigin(-1, -1));
    wgen::tests::require(
        updateSampleCount(negative) == 17 &&
            negative.regions.front().minimum ==
                wgen::LocalClipmapGridCoordinate{-5, -5},
        "negative movement should expose the opposite cache edges");
}

void testLargeMovesAndReanchorsAreDeterministic() {
    wgen::LocalClipmapConfig config{};
    config.samplesPerAxis = 9;
    config.levelCount = 2;
    config.overlapWidthCells = 1;
    config.transitionWidthCells = 1;
    const wgen::LocalClipmapCacheOrigin origin = makeOrigin(0, 0);
    const wgen::LocalClipmapCacheOrigin jump = makeOrigin(1'000'000, -1'000'000);
    const wgen::LocalClipmapUpdateSet first =
        wgen::localClipmapExposedRegions(config, origin, jump);
    const wgen::LocalClipmapUpdateSet second =
        wgen::localClipmapExposedRegions(config, origin, jump);
    wgen::tests::require(
        first.fullRefresh && first.regions.size() == 1 &&
            updateSampleCount(first) == 81 &&
            first.regions == second.regions,
        "large jumps should produce one bounded deterministic full refresh");

    const wgen::LocalClipmapUpdateSet reanchor =
        wgen::localClipmapExposedRegions(
            config,
            origin,
            makeOrigin(0, 0, {10.0, 20.0}, 2));
    wgen::tests::require(
        reanchor.fullRefresh && updateSampleCount(reanchor) == 81,
        "a local-frame reanchor should invalidate the bounded cache window");

    const glm::dvec2 anchor{100.25, -200.5};
    const glm::dvec2 largeCamera =
        anchor + glm::dvec2{1.0e12 + 0.75, -1.0e12 + 0.25};
    const wgen::LocalClipmapCacheOrigin largeFirst =
        wgen::snapLocalClipmapOrigin(config, 0, anchor, 3, largeCamera);
    const wgen::LocalClipmapCacheOrigin largeSecond =
        wgen::snapLocalClipmapOrigin(config, 0, anchor, 3, largeCamera);
    wgen::tests::require(
        largeFirst.centerSample == largeSecond.centerSample &&
            largeFirst.centerSample ==
                wgen::LocalClipmapGridCoordinate{1'000'000'000'000, -1'000'000'000'000},
        "large finite camera coordinates should snap deterministically");

    wgen::tests::requireThrows<std::overflow_error>(
        [&config] {
            wgen::snapLocalClipmapOrigin(
                config,
                0,
                {},
                0,
                {std::numeric_limits<double>::max(), 0.0});
        },
        "snapping should reject coordinates outside the integer cache range");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("config and shared patterns", testConfigAndSharedPatterns);
        wgen::tests::runTest("validation", testValidation);
        wgen::tests::runTest(
            "index bounds, winding, and partition",
            testIndexBoundsWindingAndPartition);
        wgen::tests::runTest(
            "complete multilevel coverage",
            testCompleteMultilevelCoverage);
        wgen::tests::runTest(
            "fine and coarse boundary coincidence",
            testFineAndCoarseBoundariesCoincide);
        wgen::tests::runTest("stable snapped origins", testStableSnappedOrigins);
        wgen::tests::runTest("minimal exposed regions", testMinimalExposedRegions);
        wgen::tests::runTest(
            "deterministic large moves and reanchors",
            testLargeMovesAndReanchorsAreDeterministic);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
