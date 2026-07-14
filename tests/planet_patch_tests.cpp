#include "terrain/planet/planet_patch.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr std::array PATCH_EDGES{
    wgen::PlanetPatchEdge::UMin,
    wgen::PlanetPatchEdge::UMax,
    wgen::PlanetPatchEdge::VMin,
    wgen::PlanetPatchEdge::VMax,
};

void testPatchValueTypes() {
    const wgen::PlanetPatchId first{
        .face = wgen::CubeSphereFace::Front,
        .level = 3,
        .x = 5,
        .y = 2,
    };
    const wgen::PlanetPatchId same = first;
    const wgen::PlanetPatchId different{
        .face = wgen::CubeSphereFace::Front,
        .level = 3,
        .x = 5,
        .y = 3,
    };

    wgen::tests::require(first == same, "equal patch IDs should compare equal");
    wgen::tests::require(first != different, "different patch IDs should not compare equal");
    wgen::tests::require(wgen::MAX_PLANET_PATCH_LEVEL == 24, "maximum patch level is wrong");

    const wgen::PlanetPatchBounds bounds{-1.0, 0.0, 0.0, 1.0};
    wgen::tests::require(bounds.uMin == -1.0 && bounds.vMax == 1.0, "patch bounds fields are wrong");

    const wgen::PlanetPatchNeighbor neighbor{
        .id = different,
        .touchingEdge = wgen::PlanetPatchEdge::VMin,
        .edgeCoordinateReversed = true,
    };
    wgen::tests::require(neighbor.id == different, "patch neighbor ID is wrong");
    wgen::tests::require(
        neighbor.touchingEdge == wgen::PlanetPatchEdge::VMin && neighbor.edgeCoordinateReversed,
        "patch neighbor edge metadata is wrong");

    const wgen::PlanetFaceEdgeConnection connection{
        .face = wgen::CubeSphereFace::Back,
        .edge = wgen::PlanetPatchEdge::UMax,
        .coordinateReversed = false,
    };
    wgen::tests::require(
        connection.face == wgen::CubeSphereFace::Back &&
            connection.edge == wgen::PlanetPatchEdge::UMax &&
            !connection.coordinateReversed,
        "face edge connection fields are wrong");
}

void testPatchHash() {
    const wgen::PlanetPatchId id{
        .face = wgen::CubeSphereFace::Right,
        .level = 4,
        .x = 7,
        .y = 11,
    };
    const wgen::PlanetPatchId equalId = id;

    const wgen::PlanetPatchIdHash hash;
    wgen::tests::require(hash(id) == hash(equalId), "equal patch IDs should have equal hashes");

    const std::unordered_set<wgen::PlanetPatchId, wgen::PlanetPatchIdHash> ids{id};
    wgen::tests::require(ids.contains(equalId), "patch ID hash should support unordered lookup");
}

void testPatchOrdering() {
    const std::vector<wgen::PlanetPatchId> expected{
        {wgen::CubeSphereFace::Top, 0, 0, 0},
        {wgen::CubeSphereFace::Top, 1, 0, 0},
        {wgen::CubeSphereFace::Top, 1, 0, 1},
        {wgen::CubeSphereFace::Top, 1, 1, 0},
        {wgen::CubeSphereFace::Bottom, 0, 0, 0},
    };
    std::vector<wgen::PlanetPatchId> actual{
        expected[4],
        expected[2],
        expected[0],
        expected[3],
        expected[1],
    };

    std::sort(actual.begin(), actual.end(), wgen::PlanetPatchIdLess{});
    wgen::tests::require(actual == expected, "patch ID ordering should be deterministic");
}

void testPatchesPerAxis() {
    std::uint64_t expectedPerFace = 1;
    for (std::uint8_t level = 0; level <= wgen::MAX_PLANET_PATCH_LEVEL; ++level) {
        const std::uint64_t count = wgen::patchesPerAxis(level);
        wgen::tests::require(count == (std::uint64_t{1} << level), "patch count per axis is wrong");

        wgen::tests::require(
            count <= std::numeric_limits<std::uint64_t>::max() / count,
            "test patch count per face overflows");
        const std::uint64_t patchesPerFace = count * count;
        wgen::tests::require(patchesPerFace == expectedPerFace, "patch count per face is wrong");

        wgen::tests::require(
            patchesPerFace <= std::numeric_limits<std::uint64_t>::max() / wgen::FACES.size(),
            "test planet patch count overflows");
        const std::uint64_t patchesOnPlanet = patchesPerFace * wgen::FACES.size();
        wgen::tests::require(
            patchesOnPlanet == expectedPerFace * wgen::FACES.size(),
            "planet patch count is wrong");

        if (level < wgen::MAX_PLANET_PATCH_LEVEL) {
            wgen::tests::require(
                expectedPerFace <= std::numeric_limits<std::uint64_t>::max() / 4,
                "test expected patch count overflows");
            expectedPerFace *= 4;
        }
    }

    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::patchesPerAxis(wgen::MAX_PLANET_PATCH_LEVEL + 1); },
        "patch count should reject a level above the supported maximum");
}

void testPatchValidation() {
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        const wgen::PlanetPatchId root{face, 0, 0, 0};
        wgen::tests::require(wgen::isValid(root), "root patch should be valid");
        wgen::validate(root);
    }

    const std::uint32_t maxCoordinate = wgen::patchesPerAxis(wgen::MAX_PLANET_PATCH_LEVEL) - 1;
    const wgen::PlanetPatchId maximumValid{
        wgen::CubeSphereFace::Front,
        wgen::MAX_PLANET_PATCH_LEVEL,
        maxCoordinate,
        maxCoordinate,
    };
    wgen::tests::require(wgen::isValid(maximumValid), "maximum supported patch ID should be valid");
    wgen::validate(maximumValid);

    const std::vector<wgen::PlanetPatchId> invalidIds{
        {static_cast<wgen::CubeSphereFace>(wgen::FACES.size()), 0, 0, 0},
        {wgen::CubeSphereFace::Top, static_cast<std::uint8_t>(wgen::MAX_PLANET_PATCH_LEVEL + 1), 0, 0},
        {wgen::CubeSphereFace::Top, 0, 1, 0},
        {wgen::CubeSphereFace::Top, 0, 0, 1},
        {wgen::CubeSphereFace::Bottom, 4, 16, 0},
        {wgen::CubeSphereFace::Bottom, 4, 0, 16},
    };

    for (const wgen::PlanetPatchId& id : invalidIds) {
        wgen::tests::require(!wgen::isValid(id), "invalid patch ID should fail non-throwing validation");
        wgen::tests::requireThrows<std::invalid_argument>(
            [&id] { wgen::validate(id); },
            "invalid patch ID should be rejected");
    }
}

void testPatchHierarchy() {
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        wgen::tests::require(
            !wgen::parent({face, 0, 0, 0}).has_value(),
            "root patch should not have a parent");

        for (std::uint8_t level = 0; level < 6; ++level) {
            const std::uint32_t count = wgen::patchesPerAxis(level);
            for (std::uint32_t y = 0; y < count; ++y) {
                for (std::uint32_t x = 0; x < count; ++x) {
                    const wgen::PlanetPatchId id{face, level, x, y};
                    const std::array<wgen::PlanetPatchId, 4> actual = wgen::children(id);
                    const std::array<wgen::PlanetPatchId, 4> expected{
                        wgen::child(id, 0, 0),
                        wgen::child(id, 1, 0),
                        wgen::child(id, 0, 1),
                        wgen::child(id, 1, 1),
                    };

                    wgen::tests::require(actual == expected, "patch children have the wrong stable order");
                    for (const wgen::PlanetPatchId& childId : actual) {
                        wgen::tests::require(
                            wgen::parent(childId) == id,
                            "child patch should map back to its parent");
                    }
                }
            }
        }
    }
}

void testPatchEdgeChildren() {
    const wgen::PlanetPatchId id{wgen::CubeSphereFace::Left, 3, 5, 2};
    const std::array<wgen::PlanetPatchId, 4> all = wgen::children(id);

    wgen::tests::require(
        wgen::childrenTouchingEdge(id, wgen::PlanetPatchEdge::UMin) ==
            std::array<wgen::PlanetPatchId, 2>{all[0], all[2]},
        "UMin children should be ordered by increasing v");
    wgen::tests::require(
        wgen::childrenTouchingEdge(id, wgen::PlanetPatchEdge::UMax) ==
            std::array<wgen::PlanetPatchId, 2>{all[1], all[3]},
        "UMax children should be ordered by increasing v");
    wgen::tests::require(
        wgen::childrenTouchingEdge(id, wgen::PlanetPatchEdge::VMin) ==
            std::array<wgen::PlanetPatchId, 2>{all[0], all[1]},
        "VMin children should be ordered by increasing u");
    wgen::tests::require(
        wgen::childrenTouchingEdge(id, wgen::PlanetPatchEdge::VMax) ==
            std::array<wgen::PlanetPatchId, 2>{all[2], all[3]},
        "VMax children should be ordered by increasing u");
}

void testPatchHierarchyValidation() {
    const wgen::PlanetPatchId id{wgen::CubeSphereFace::Top, 3, 2, 4};
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] { wgen::child(id, 2, 0); },
        "child should reject an x bit above one");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] { wgen::child(id, 0, 2); },
        "child should reject a y bit above one");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] {
            wgen::childrenTouchingEdge(id, static_cast<wgen::PlanetPatchEdge>(4));
        },
        "edge children should reject an invalid edge");

    const std::uint32_t maxCoordinate = wgen::patchesPerAxis(wgen::MAX_PLANET_PATCH_LEVEL) - 1;
    const wgen::PlanetPatchId maximumPatch{
        wgen::CubeSphereFace::Bottom,
        wgen::MAX_PLANET_PATCH_LEVEL,
        maxCoordinate,
        maxCoordinate,
    };
    wgen::tests::requireThrows<std::overflow_error>(
        [&maximumPatch] { wgen::child(maximumPatch, 0, 0); },
        "child should reject subdivision at the maximum level");
    wgen::tests::requireThrows<std::overflow_error>(
        [&maximumPatch] { wgen::children(maximumPatch); },
        "children should reject subdivision at the maximum level");
    wgen::tests::requireThrows<std::overflow_error>(
        [&maximumPatch] {
            wgen::childrenTouchingEdge(maximumPatch, wgen::PlanetPatchEdge::UMin);
        },
        "edge children should reject subdivision at the maximum level");
}

void requireValidBounds(const wgen::PlanetPatchBounds& bounds) {
    wgen::tests::require(bounds.uMin >= -1.0 && bounds.uMax <= 1.0, "patch u bounds are outside the face");
    wgen::tests::require(bounds.vMin >= -1.0 && bounds.vMax <= 1.0, "patch v bounds are outside the face");
    wgen::tests::require(bounds.uMin < bounds.uMax, "patch u bounds should have positive width");
    wgen::tests::require(bounds.vMin < bounds.vMax, "patch v bounds should have positive height");
}

void testPatchBounds() {
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        const wgen::PlanetPatchBounds root = wgen::patchBounds({face, 0, 0, 0});
        wgen::tests::require(
            root.uMin == -1.0 && root.uMax == 1.0 && root.vMin == -1.0 && root.vMax == 1.0,
            "root patch should cover its complete face");

        for (std::uint8_t level = 0; level <= 7; ++level) {
            const std::uint32_t count = wgen::patchesPerAxis(level);
            for (std::uint32_t y = 0; y < count; ++y) {
                for (std::uint32_t x = 0; x < count; ++x) {
                    const wgen::PlanetPatchBounds bounds = wgen::patchBounds({face, level, x, y});
                    requireValidBounds(bounds);

                    if (x + 1 < count) {
                        const wgen::PlanetPatchBounds right = wgen::patchBounds({face, level, x + 1, y});
                        wgen::tests::require(
                            bounds.uMax == right.uMin,
                            "horizontally adjacent patches should share an exact boundary");
                    }
                    if (y + 1 < count) {
                        const wgen::PlanetPatchBounds above = wgen::patchBounds({face, level, x, y + 1});
                        wgen::tests::require(
                            bounds.vMax == above.vMin,
                            "vertically adjacent patches should share an exact boundary");
                    }
                }
            }
        }

        for (std::uint8_t level = 8; level <= wgen::MAX_PLANET_PATCH_LEVEL; ++level) {
            const std::uint32_t count = wgen::patchesPerAxis(level);
            const std::array<std::uint32_t, 3> coordinates{0, count / 2, count - 1};
            for (const std::uint32_t y : coordinates) {
                for (const std::uint32_t x : coordinates) {
                    requireValidBounds(wgen::patchBounds({face, level, x, y}));
                }
            }

            const std::uint32_t lowerMiddle = count / 2 - 1;
            const wgen::PlanetPatchBounds left = wgen::patchBounds({face, level, lowerMiddle, lowerMiddle});
            const wgen::PlanetPatchBounds right = wgen::patchBounds({face, level, lowerMiddle + 1, lowerMiddle});
            const wgen::PlanetPatchBounds above = wgen::patchBounds({face, level, lowerMiddle, lowerMiddle + 1});
            wgen::tests::require(left.uMax == right.uMin, "deep adjacent patches should share a u boundary");
            wgen::tests::require(left.vMax == above.vMin, "deep adjacent patches should share a v boundary");
        }
    }
}

void testChildBoundsCoverParent() {
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (std::uint8_t level = 0; level <= 7; ++level) {
            const std::uint32_t count = wgen::patchesPerAxis(level);
            const std::array<wgen::PlanetPatchId, 3> ids{
                wgen::PlanetPatchId{face, level, 0, 0},
                wgen::PlanetPatchId{face, level, count / 2, count / 2},
                wgen::PlanetPatchId{face, level, count - 1, count - 1},
            };

            for (const wgen::PlanetPatchId& id : ids) {
                const wgen::PlanetPatchBounds bounds = wgen::patchBounds(id);
                const std::array<wgen::PlanetPatchId, 4> childIds = wgen::children(id);
                const wgen::PlanetPatchBounds lowerLeft = wgen::patchBounds(childIds[0]);
                const wgen::PlanetPatchBounds lowerRight = wgen::patchBounds(childIds[1]);
                const wgen::PlanetPatchBounds upperLeft = wgen::patchBounds(childIds[2]);
                const wgen::PlanetPatchBounds upperRight = wgen::patchBounds(childIds[3]);

                wgen::tests::require(
                    lowerLeft.uMin == bounds.uMin && lowerLeft.vMin == bounds.vMin,
                    "first child should start at the parent minimum");
                wgen::tests::require(
                    upperRight.uMax == bounds.uMax && upperRight.vMax == bounds.vMax,
                    "last child should end at the parent maximum");
                wgen::tests::require(
                    lowerLeft.uMax == lowerRight.uMin && upperLeft.uMax == upperRight.uMin,
                    "child columns should share the parent u midpoint");
                wgen::tests::require(
                    lowerLeft.vMax == upperLeft.vMin && lowerRight.vMax == upperRight.vMin,
                    "child rows should share the parent v midpoint");
            }
        }
    }
}

void testPatchBoundsValidation() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] { wgen::patchBounds({wgen::CubeSphereFace::Front, 2, 4, 0}); },
        "patch bounds should reject an invalid patch ID");
}

struct ExpectedFaceEdgeConnection {
    wgen::CubeSphereFace sourceFace;
    wgen::PlanetPatchEdge sourceEdge;
    wgen::CubeSphereFace targetFace;
    wgen::PlanetPatchEdge targetEdge;
    bool reversed;
};

constexpr std::array EXPECTED_FACE_EDGE_CONNECTIONS{
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::UMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::UMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::VMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::VMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::UMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::UMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::VMin, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::VMax, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::UMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::UMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::UMax, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::UMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::UMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::UMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::UMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::UMin, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::VMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::VMin, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::VMin, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Back, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::VMin, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::UMin, wgen::CubeSphereFace::Left, wgen::PlanetPatchEdge::VMax, true},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::UMax, wgen::CubeSphereFace::Right, wgen::PlanetPatchEdge::VMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::VMin, wgen::CubeSphereFace::Top, wgen::PlanetPatchEdge::VMax, false},
    ExpectedFaceEdgeConnection{wgen::CubeSphereFace::Front, wgen::PlanetPatchEdge::VMax, wgen::CubeSphereFace::Bottom, wgen::PlanetPatchEdge::VMax, true},
};

void testFaceEdgeConnections() {
    for (const ExpectedFaceEdgeConnection& expected : EXPECTED_FACE_EDGE_CONNECTIONS) {
        const wgen::PlanetFaceEdgeConnection actual =
            wgen::faceEdgeConnection(expected.sourceFace, expected.sourceEdge);
        wgen::tests::require(actual.face == expected.targetFace, "face edge target face is wrong");
        wgen::tests::require(actual.edge == expected.targetEdge, "face edge target edge is wrong");
        wgen::tests::require(
            actual.coordinateReversed == expected.reversed,
            "face edge coordinate direction is wrong");
    }
}

void testFaceEdgeConnectionsAreReciprocal() {
    using EdgeEndpoint = std::pair<wgen::CubeSphereFace, wgen::PlanetPatchEdge>;
    using UndirectedEdge = std::pair<EdgeEndpoint, EdgeEndpoint>;
    std::set<UndirectedEdge> undirectedEdges;

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const wgen::PlanetPatchEdge edge : PATCH_EDGES) {
            const wgen::PlanetFaceEdgeConnection connection = wgen::faceEdgeConnection(face, edge);
            const wgen::PlanetFaceEdgeConnection reciprocal =
                wgen::faceEdgeConnection(connection.face, connection.edge);
            wgen::tests::require(reciprocal.face == face, "reciprocal edge should return to the source face");
            wgen::tests::require(reciprocal.edge == edge, "reciprocal edge should return to the source edge");
            wgen::tests::require(
                reciprocal.coordinateReversed == connection.coordinateReversed,
                "reciprocal edge should preserve the reversal flag");

            EdgeEndpoint source{face, edge};
            EdgeEndpoint target{connection.face, connection.edge};
            if (target < source) {
                std::swap(source, target);
            }
            undirectedEdges.emplace(source, target);
        }
    }

    wgen::tests::require(undirectedEdges.size() == 12, "cube should have exactly 12 undirected edges");
}

void testFaceEdgeConnectionValidation() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::faceEdgeConnection(
                static_cast<wgen::CubeSphereFace>(wgen::FACES.size()),
                wgen::PlanetPatchEdge::UMin);
        },
        "face edge connection should reject an invalid face");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::faceEdgeConnection(
                wgen::CubeSphereFace::Top,
                static_cast<wgen::PlanetPatchEdge>(4));
        },
        "face edge connection should reject an invalid edge");
}

void testInteriorSameLevelNeighbors() {
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        const wgen::PlanetPatchId id{face, 3, 3, 4};
        const std::array<wgen::PlanetPatchNeighbor, 4> expected{
            wgen::PlanetPatchNeighbor{{face, 3, 2, 4}, wgen::PlanetPatchEdge::UMax, false},
            wgen::PlanetPatchNeighbor{{face, 3, 4, 4}, wgen::PlanetPatchEdge::UMin, false},
            wgen::PlanetPatchNeighbor{{face, 3, 3, 3}, wgen::PlanetPatchEdge::VMax, false},
            wgen::PlanetPatchNeighbor{{face, 3, 3, 5}, wgen::PlanetPatchEdge::VMin, false},
        };

        for (std::size_t i = 0; i < PATCH_EDGES.size(); ++i) {
            const wgen::PlanetPatchNeighbor actual = wgen::sameLevelNeighbor(id, PATCH_EDGES[i]);
            wgen::tests::require(actual.id == expected[i].id, "interior neighbor patch ID is wrong");
            wgen::tests::require(
                actual.touchingEdge == expected[i].touchingEdge,
                "interior neighbor touching edge is wrong");
            wgen::tests::require(
                !actual.edgeCoordinateReversed,
                "interior neighbor should not reverse its edge coordinate");

            const wgen::PlanetPatchNeighbor back =
                wgen::sameLevelNeighbor(actual.id, actual.touchingEdge);
            wgen::tests::require(back.id == id, "interior neighbor traversal should be reciprocal");
            wgen::tests::require(back.touchingEdge == PATCH_EDGES[i], "interior reciprocal edge is wrong");
        }
    }
}

wgen::PlanetPatchId boundaryPatch(
        wgen::CubeSphereFace face,
        std::uint8_t level,
        wgen::PlanetPatchEdge edge,
        std::uint32_t edgeCoordinate) {
    const std::uint32_t last = wgen::patchesPerAxis(level) - 1;
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin: return {face, level, 0, edgeCoordinate};
        case wgen::PlanetPatchEdge::UMax: return {face, level, last, edgeCoordinate};
        case wgen::PlanetPatchEdge::VMin: return {face, level, edgeCoordinate, 0};
        case wgen::PlanetPatchEdge::VMax: return {face, level, edgeCoordinate, last};
    }
    throw std::invalid_argument{"test patch edge is invalid"};
}

wgen::PlanetPatchId expectedCrossFaceNeighbor(
        const wgen::PlanetPatchId& source,
        wgen::PlanetPatchEdge sourceEdge,
        const wgen::PlanetFaceEdgeConnection& connection) {
    const std::uint32_t count = wgen::patchesPerAxis(source.level);
    const std::uint32_t sourceCoordinate =
        sourceEdge == wgen::PlanetPatchEdge::UMin || sourceEdge == wgen::PlanetPatchEdge::UMax
        ? source.y
        : source.x;
    const std::uint32_t targetCoordinate = connection.coordinateReversed
        ? count - 1 - sourceCoordinate
        : sourceCoordinate;
    return boundaryPatch(connection.face, source.level, connection.edge, targetCoordinate);
}

void testBoundarySameLevelNeighbors() {
    for (std::uint8_t level = 0; level <= 5; ++level) {
        const std::uint32_t count = wgen::patchesPerAxis(level);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (const wgen::PlanetPatchEdge edge : PATCH_EDGES) {
                const wgen::PlanetFaceEdgeConnection connection = wgen::faceEdgeConnection(face, edge);
                for (std::uint32_t edgeCoordinate = 0; edgeCoordinate < count; ++edgeCoordinate) {
                    const wgen::PlanetPatchId id = boundaryPatch(face, level, edge, edgeCoordinate);
                    const wgen::PlanetPatchNeighbor actual = wgen::sameLevelNeighbor(id, edge);
                    const wgen::PlanetPatchId expected = expectedCrossFaceNeighbor(id, edge, connection);

                    wgen::tests::require(actual.id == expected, "cross-face neighbor patch ID is wrong");
                    wgen::tests::require(
                        actual.touchingEdge == connection.edge,
                        "cross-face neighbor touching edge is wrong");
                    wgen::tests::require(
                        actual.edgeCoordinateReversed == connection.coordinateReversed,
                        "cross-face neighbor reversal flag is wrong");

                    const wgen::PlanetPatchNeighbor back =
                        wgen::sameLevelNeighbor(actual.id, actual.touchingEdge);
                    wgen::tests::require(back.id == id, "cross-face neighbor traversal should be reciprocal");
                    wgen::tests::require(back.touchingEdge == edge, "cross-face reciprocal edge is wrong");
                    wgen::tests::require(
                        back.edgeCoordinateReversed == actual.edgeCoordinateReversed,
                        "cross-face reciprocal reversal flag is wrong");
                }
            }
        }
    }
}

wgen::PlanetPatchEdge otherCornerEdge(
        const wgen::PlanetPatchId& id,
        wgen::PlanetPatchEdge excludedEdge) {
    const std::uint32_t last = wgen::patchesPerAxis(id.level) - 1;
    const wgen::PlanetPatchEdge uEdge = id.x == 0
        ? wgen::PlanetPatchEdge::UMin
        : wgen::PlanetPatchEdge::UMax;
    const wgen::PlanetPatchEdge vEdge = id.y == 0
        ? wgen::PlanetPatchEdge::VMin
        : wgen::PlanetPatchEdge::VMax;
    wgen::tests::require(id.x == 0 || id.x == last, "test patch should touch a u boundary");
    wgen::tests::require(id.y == 0 || id.y == last, "test patch should touch a v boundary");
    wgen::tests::require(
        excludedEdge == uEdge || excludedEdge == vEdge,
        "excluded edge should touch the test patch corner");
    return excludedEdge == uEdge ? vEdge : uEdge;
}

void testSameLevelNeighborCorners() {
    for (std::uint8_t level = 1; level <= 5; ++level) {
        const std::uint32_t last = wgen::patchesPerAxis(level) - 1;
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (const bool useUMax : {false, true}) {
                for (const bool useVMax : {false, true}) {
                    const wgen::PlanetPatchId corner{
                        face,
                        level,
                        useUMax ? last : 0,
                        useVMax ? last : 0,
                    };
                    const wgen::PlanetPatchEdge uEdge = useUMax
                        ? wgen::PlanetPatchEdge::UMax
                        : wgen::PlanetPatchEdge::UMin;
                    const wgen::PlanetPatchEdge vEdge = useVMax
                        ? wgen::PlanetPatchEdge::VMax
                        : wgen::PlanetPatchEdge::VMin;
                    const wgen::PlanetPatchNeighbor acrossU = wgen::sameLevelNeighbor(corner, uEdge);
                    const wgen::PlanetPatchNeighbor acrossV = wgen::sameLevelNeighbor(corner, vEdge);

                    const wgen::PlanetPatchNeighbor uToV = wgen::sameLevelNeighbor(
                        acrossU.id,
                        otherCornerEdge(acrossU.id, acrossU.touchingEdge));
                    const wgen::PlanetPatchNeighbor vToU = wgen::sameLevelNeighbor(
                        acrossV.id,
                        otherCornerEdge(acrossV.id, acrossV.touchingEdge));
                    wgen::tests::require(
                        uToV.id == acrossV.id,
                        "cube corner traversal from the u neighbor should reach the v neighbor");
                    wgen::tests::require(
                        vToU.id == acrossU.id,
                        "cube corner traversal from the v neighbor should reach the u neighbor");
                }
            }
        }
    }
}

void testSameLevelNeighborValidation() {
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::sameLevelNeighbor(
                {wgen::CubeSphereFace::Top, 2, 4, 0},
                wgen::PlanetPatchEdge::UMin);
        },
        "same-level neighbor should reject an invalid patch ID");
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::sameLevelNeighbor(
                {wgen::CubeSphereFace::Top, 0, 0, 0},
                static_cast<wgen::PlanetPatchEdge>(4));
        },
        "same-level neighbor should reject an invalid edge");
}

void requireDirectionNear(
        const glm::dvec3& actual,
        const glm::dvec3& expected,
        std::string_view message) {
    wgen::tests::require(glm::length(actual - expected) <= 0.000001, message);
}

void requireValidDirection(const glm::dvec3& direction) {
    wgen::tests::require(
        std::isfinite(direction.x) && std::isfinite(direction.y) && std::isfinite(direction.z),
        "patch sample direction should be finite");
    wgen::tests::require(
        std::abs(glm::length(direction) - 1.0) <= 0.000001,
        "patch sample direction should have unit length");
}

void testPatchSurfaceSamples() {
    constexpr std::array<std::uint32_t, 3> QUAD_COUNTS{1, 7, 32};
    constexpr std::array<std::uint8_t, 3> LEVELS{0, 4, wgen::MAX_PLANET_PATCH_LEVEL};

    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const std::uint8_t level : LEVELS) {
            const std::uint32_t count = wgen::patchesPerAxis(level);
            const wgen::PlanetPatchId id{face, level, count / 3, count - 1};
            const wgen::PlanetPatchBounds bounds = wgen::patchBounds(id);

            for (const std::uint32_t quadCount : QUAD_COUNTS) {
                const wgen::PlanetSurfaceSample minimum =
                    wgen::patchSurfaceSample(id, quadCount, 0, 0);
                const wgen::PlanetSurfaceSample maximum =
                    wgen::patchSurfaceSample(id, quadCount, quadCount, quadCount);
                const wgen::PlanetSurfaceSample repeated =
                    wgen::patchSurfaceSample(id, quadCount, 0, 0);

                wgen::tests::require(minimum.face == face, "patch sample face is wrong");
                wgen::tests::require(
                    minimum.u == bounds.uMin && minimum.v == bounds.vMin,
                    "patch minimum sample should match patch bounds");
                wgen::tests::require(
                    maximum.u == bounds.uMax && maximum.v == bounds.vMax,
                    "patch maximum sample should match patch bounds");
                wgen::tests::require(
                    repeated.u == minimum.u && repeated.v == minimum.v,
                    "patch sample UV should be deterministic");
                wgen::tests::require(
                    repeated.direction.x == minimum.direction.x &&
                        repeated.direction.y == minimum.direction.y &&
                        repeated.direction.z == minimum.direction.z,
                    "patch sample direction should be deterministic");
                requireValidDirection(minimum.direction);
                requireValidDirection(maximum.direction);
            }
        }
    }

    const std::uint32_t last = wgen::patchesPerAxis(wgen::MAX_PLANET_PATCH_LEVEL) - 1;
    const std::uint32_t maximumQuadCount = std::numeric_limits<std::uint32_t>::max();
    const wgen::PlanetSurfaceSample maximumAddress = wgen::patchSurfaceSample(
        {wgen::CubeSphereFace::Front, wgen::MAX_PLANET_PATCH_LEVEL, last, last},
        maximumQuadCount,
        maximumQuadCount,
        maximumQuadCount);
    wgen::tests::require(
        maximumAddress.u == 1.0 && maximumAddress.v == 1.0,
        "maximum patch sample address should not overflow");
}

wgen::PlanetSurfaceSample patchEdgeSample(
        const wgen::PlanetPatchId& id,
        wgen::PlanetPatchEdge edge,
        std::uint32_t quadCount,
        std::uint32_t edgeSample) {
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin:
            return wgen::patchSurfaceSample(id, quadCount, 0, edgeSample);
        case wgen::PlanetPatchEdge::UMax:
            return wgen::patchSurfaceSample(id, quadCount, quadCount, edgeSample);
        case wgen::PlanetPatchEdge::VMin:
            return wgen::patchSurfaceSample(id, quadCount, edgeSample, 0);
        case wgen::PlanetPatchEdge::VMax:
            return wgen::patchSurfaceSample(id, quadCount, edgeSample, quadCount);
    }
    throw std::invalid_argument{"test patch edge is invalid"};
}

void testSameFacePatchSampleSeams() {
    constexpr std::uint32_t QUAD_COUNT = 32;
    for (std::uint8_t level = 1; level <= 5; ++level) {
        const std::uint32_t count = wgen::patchesPerAxis(level);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (std::uint32_t coordinate = 0; coordinate + 1 < count; ++coordinate) {
                const wgen::PlanetPatchId left{face, level, coordinate, count / 2};
                const wgen::PlanetPatchId right{face, level, coordinate + 1, count / 2};
                const wgen::PlanetPatchId lower{face, level, count / 2, coordinate};
                const wgen::PlanetPatchId upper{face, level, count / 2, coordinate + 1};

                for (std::uint32_t sample = 0; sample <= QUAD_COUNT; ++sample) {
                    const wgen::PlanetSurfaceSample leftSample =
                        patchEdgeSample(left, wgen::PlanetPatchEdge::UMax, QUAD_COUNT, sample);
                    const wgen::PlanetSurfaceSample rightSample =
                        patchEdgeSample(right, wgen::PlanetPatchEdge::UMin, QUAD_COUNT, sample);
                    const wgen::PlanetSurfaceSample lowerSample =
                        patchEdgeSample(lower, wgen::PlanetPatchEdge::VMax, QUAD_COUNT, sample);
                    const wgen::PlanetSurfaceSample upperSample =
                        patchEdgeSample(upper, wgen::PlanetPatchEdge::VMin, QUAD_COUNT, sample);

                    wgen::tests::require(
                        leftSample.u == rightSample.u && leftSample.v == rightSample.v,
                        "same-face u-edge samples should have identical UV coordinates");
                    wgen::tests::require(
                        lowerSample.u == upperSample.u && lowerSample.v == upperSample.v,
                        "same-face v-edge samples should have identical UV coordinates");
                    requireDirectionNear(
                        leftSample.direction,
                        rightSample.direction,
                        "same-face u-edge sample directions should match");
                    requireDirectionNear(
                        lowerSample.direction,
                        upperSample.direction,
                        "same-face v-edge sample directions should match");
                }
            }
        }
    }
}

void testCrossFacePatchSampleSeams() {
    constexpr std::uint32_t QUAD_COUNT = 32;
    constexpr std::array<std::uint8_t, 4> LEVELS{0, 1, 3, 5};

    for (const std::uint8_t level : LEVELS) {
        const std::uint32_t count = wgen::patchesPerAxis(level);
        for (const wgen::CubeSphereFace face : wgen::FACES) {
            for (const wgen::PlanetPatchEdge edge : PATCH_EDGES) {
                for (std::uint32_t patchCoordinate = 0; patchCoordinate < count; ++patchCoordinate) {
                    const wgen::PlanetPatchId id = boundaryPatch(face, level, edge, patchCoordinate);
                    const wgen::PlanetPatchNeighbor neighbor = wgen::sameLevelNeighbor(id, edge);

                    for (std::uint32_t sample = 0; sample <= QUAD_COUNT; ++sample) {
                        const std::uint32_t targetSample = neighbor.edgeCoordinateReversed
                            ? QUAD_COUNT - sample
                            : sample;
                        const wgen::PlanetSurfaceSample source =
                            patchEdgeSample(id, edge, QUAD_COUNT, sample);
                        const wgen::PlanetSurfaceSample target = patchEdgeSample(
                            neighbor.id,
                            neighbor.touchingEdge,
                            QUAD_COUNT,
                            targetSample);
                        requireValidDirection(source.direction);
                        requireDirectionNear(
                            source.direction,
                            target.direction,
                            "cross-face edge sample directions should match");
                    }
                }
            }
        }
    }
}

void testPatchSurfaceSampleValidation() {
    const wgen::PlanetPatchId id{wgen::CubeSphereFace::Top, 2, 1, 1};
    wgen::tests::requireThrows<std::invalid_argument>(
        [] {
            wgen::patchSurfaceSample(
                {wgen::CubeSphereFace::Top, 2, 4, 0},
                32,
                0,
                0);
        },
        "patch sampling should reject an invalid patch ID");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] { wgen::patchSurfaceSample(id, 0, 0, 0); },
        "patch sampling should reject zero quads");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] { wgen::patchSurfaceSample(id, 32, 33, 0); },
        "patch sampling should reject a local x coordinate beyond the patch");
    wgen::tests::requireThrows<std::invalid_argument>(
        [&id] { wgen::patchSurfaceSample(id, 32, 0, 33); },
        "patch sampling should reject a local y coordinate beyond the patch");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("planet patch value types", testPatchValueTypes);
        wgen::tests::runTest("planet patch hash", testPatchHash);
        wgen::tests::runTest("planet patch ordering", testPatchOrdering);
        wgen::tests::runTest("planet patches per axis", testPatchesPerAxis);
        wgen::tests::runTest("planet patch validation", testPatchValidation);
        wgen::tests::runTest("planet patch hierarchy", testPatchHierarchy);
        wgen::tests::runTest("planet patch edge children", testPatchEdgeChildren);
        wgen::tests::runTest("planet patch hierarchy validation", testPatchHierarchyValidation);
        wgen::tests::runTest("planet patch bounds", testPatchBounds);
        wgen::tests::runTest("planet child bounds cover parent", testChildBoundsCoverParent);
        wgen::tests::runTest("planet patch bounds validation", testPatchBoundsValidation);
        wgen::tests::runTest("planet face edge connections", testFaceEdgeConnections);
        wgen::tests::runTest("planet face edge reciprocity", testFaceEdgeConnectionsAreReciprocal);
        wgen::tests::runTest("planet face edge validation", testFaceEdgeConnectionValidation);
        wgen::tests::runTest("planet interior same-level neighbors", testInteriorSameLevelNeighbors);
        wgen::tests::runTest("planet boundary same-level neighbors", testBoundarySameLevelNeighbors);
        wgen::tests::runTest("planet same-level neighbor corners", testSameLevelNeighborCorners);
        wgen::tests::runTest("planet same-level neighbor validation", testSameLevelNeighborValidation);
        wgen::tests::runTest("planet patch surface samples", testPatchSurfaceSamples);
        wgen::tests::runTest("planet same-face sample seams", testSameFacePatchSampleSeams);
        wgen::tests::runTest("planet cross-face sample seams", testCrossFacePatchSampleSeams);
        wgen::tests::runTest("planet patch surface sample validation", testPatchSurfaceSampleValidation);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
