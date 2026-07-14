#include "terrain/planet/planet_patch.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

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
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
