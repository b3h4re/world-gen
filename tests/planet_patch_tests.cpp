#include "terrain/planet/planet_patch.hpp"

#include "helpers.hpp"

#include <algorithm>
#include <iostream>
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

} // namespace

int main() {
    try {
        wgen::tests::runTest("planet patch value types", testPatchValueTypes);
        wgen::tests::runTest("planet patch hash", testPatchHash);
        wgen::tests::runTest("planet patch ordering", testPatchOrdering);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
