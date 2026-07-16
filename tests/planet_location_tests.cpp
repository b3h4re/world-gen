#include "terrain/planet/planet_location.hpp"
#include "terrain/planet/planet_patch.hpp"

#include "helpers.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

void requireVectorNear(
        glm::dvec3 actual,
        glm::dvec3 expected,
        double tolerance,
        const std::string& message) {
    wgen::tests::require(
        glm::length(actual - expected) <= tolerance,
        message);
}

glm::dvec2 edgeUv(wgen::PlanetPatchEdge edge, double coordinate) {
    switch (edge) {
        case wgen::PlanetPatchEdge::UMin: return {-1.0, coordinate};
        case wgen::PlanetPatchEdge::UMax: return {1.0, coordinate};
        case wgen::PlanetPatchEdge::VMin: return {coordinate, -1.0};
        case wgen::PlanetPatchEdge::VMax: return {coordinate, 1.0};
    }
    throw std::invalid_argument{"unknown planet patch edge"};
}

void testDoubleProjectionAndCompatibilityWrappers() {
    requireVectorNear(
        wgen::cubeSphereDirection(wgen::CubeSphereFace::Top, 0.0, 0.0),
        {0.0, 0.0, 1.0},
        0.000000000000001,
        "top face center should project to +Z");
    requireVectorNear(
        wgen::cubeSphereDirection(wgen::CubeSphereFace::Front, 0.0, 0.0),
        {0.0, 1.0, 0.0},
        0.000000000000001,
        "front face center should project to +Y");

    constexpr std::array UV_VALUES{-1.0, -0.75, -0.125, 0.0, 0.5, 1.0};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const double u : UV_VALUES) {
            for (const double v : UV_VALUES) {
                const glm::dvec3 direction =
                    wgen::cubeSphereDirection(face, u, v);
                wgen::tests::require(
                    std::abs(glm::length(direction) - 1.0) <=
                        0.000000000000001,
                    "double cube projection should remain on the unit sphere");
                const glm::vec3 compatibility = wgen::cubeSphereDirection(
                    face,
                    static_cast<float>(u),
                    static_cast<float>(v));
                requireVectorNear(
                    glm::dvec3{compatibility},
                    direction,
                    0.0000002,
                    "float projection should wrap the canonical double path");
            }
        }
    }
}

void testDirectionFaceUvRoundTrips() {
    constexpr std::array UV_VALUES{-1.0, -0.8, -0.1, 0.0, 0.65, 1.0};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const double u : UV_VALUES) {
            for (const double v : UV_VALUES) {
                const glm::dvec3 direction =
                    wgen::cubeSphereDirection(face, u, v);
                const wgen::CubeSphereFaceUv cached =
                    wgen::directionToCubeSphereFaceUv(direction, face);
                wgen::tests::require(
                    cached.face == face &&
                        std::abs(cached.u - u) <= 0.000000001 &&
                        std::abs(cached.v - v) <= 0.000000001,
                    "a preferred face should preserve edge and corner UV identity");
                requireVectorNear(
                    wgen::cubeSphereDirection(cached.face, cached.u, cached.v),
                    direction,
                    0.000000001,
                    "preferred face/UV should reconstruct its direction");

                const wgen::CubeSphereFaceUv canonical =
                    wgen::directionToCubeSphereFaceUv(direction);
                requireVectorNear(
                    wgen::cubeSphereDirection(
                        canonical.face,
                        canonical.u,
                        canonical.v),
                    direction,
                    0.000000001,
                    "canonical direction conversion should round-trip across seams");
            }
        }
    }
}

void testEveryFaceSeamAndCorner() {
    constexpr std::array EDGES{
        wgen::PlanetPatchEdge::UMin,
        wgen::PlanetPatchEdge::UMax,
        wgen::PlanetPatchEdge::VMin,
        wgen::PlanetPatchEdge::VMax,
    };
    constexpr std::array EDGE_COORDINATES{-1.0, -0.7, 0.0, 0.4, 1.0};
    for (const wgen::CubeSphereFace face : wgen::FACES) {
        for (const wgen::PlanetPatchEdge edge : EDGES) {
            const wgen::PlanetFaceEdgeConnection connection =
                wgen::faceEdgeConnection(face, edge);
            for (const double coordinate : EDGE_COORDINATES) {
                const glm::dvec2 sourceUv = edgeUv(edge, coordinate);
                const glm::dvec2 connectedUv = edgeUv(
                    connection.edge,
                    connection.coordinateReversed ? -coordinate : coordinate);
                const glm::dvec3 sourceDirection = wgen::cubeSphereDirection(
                    face,
                    sourceUv.x,
                    sourceUv.y);
                const glm::dvec3 connectedDirection = wgen::cubeSphereDirection(
                    connection.face,
                    connectedUv.x,
                    connectedUv.y);
                requireVectorNear(
                    sourceDirection,
                    connectedDirection,
                    0.000000000000001,
                    "double cube projection should be identical across every face seam");

                const wgen::CubeSphereFaceUv sourceCache =
                    wgen::directionToCubeSphereFaceUv(sourceDirection, face);
                const wgen::CubeSphereFaceUv connectedCache =
                    wgen::directionToCubeSphereFaceUv(
                        sourceDirection,
                        connection.face);
                wgen::tests::require(
                    sourceCache.face == face &&
                        connectedCache.face == connection.face,
                    "face preference should retain either valid side of a seam");
            }
        }
    }
}

void testMaximumLodCoordinatesRemainDistinct() {
    const std::uint32_t count =
        wgen::patchesPerAxis(wgen::MAX_PLANET_PATCH_LEVEL);
    const wgen::PlanetPatchId id{
        wgen::CubeSphereFace::Top,
        wgen::MAX_PLANET_PATCH_LEVEL,
        count - 1,
        count - 1,
    };
    constexpr std::uint32_t QUADS = 32;
    const wgen::PlanetSurfaceSample first =
        wgen::patchSurfaceSample(id, QUADS, 29, 30);
    const wgen::PlanetSurfaceSample second =
        wgen::patchSurfaceSample(id, QUADS, 30, 30);

    wgen::tests::require(
        first.u != second.u && first.direction != second.direction,
        "adjacent maximum-LOD global samples should remain distinguishable");
    wgen::tests::require(
        glm::length(first.direction - second.direction) >
            std::numeric_limits<double>::epsilon(),
        "maximum-LOD directions should retain a non-zero double separation");
    constexpr double EARTH_RADIUS_METERS = 6371000.0;
    wgen::tests::require(
        glm::length(
            first.direction * EARTH_RADIUS_METERS -
            second.direction * EARTH_RADIUS_METERS) > 0.001,
        "maximum-LOD samples should retain distinguishable physical positions");
}

void testPlanetLocationAndTangentFrame() {
    const glm::dvec3 direction = wgen::cubeSphereDirection(
        wgen::CubeSphereFace::Right,
        0.37,
        -0.42);
    const wgen::PlanetOrientation orientation{
        .headingRadians = 1.25,
        .pitchRadians = -0.35,
        .rollRadians = 0.1,
    };
    const wgen::PlanetLocation location = wgen::makePlanetLocation(
        direction,
        125.25,
        orientation,
        wgen::CubeSphereFace::Right);
    wgen::tests::require(
        location.faceUv &&
            location.faceUv->face == wgen::CubeSphereFace::Right &&
            location.orientation == orientation,
        "planet location should retain physical orientation and its face cache");

    constexpr double RADIUS = 6371000.0;
    const glm::dvec3 globalPosition =
        wgen::planetLocationPositionMeters(location, RADIUS);
    wgen::tests::require(
        std::abs(glm::length(globalPosition) - (RADIUS + 125.25)) <=
            0.00000001,
        "planet location altitude should remain in physical units");
    requireVectorNear(
        wgen::planetLocationPositionInPlanetRadii(location, RADIUS),
        globalPosition / RADIUS,
        0.000000000000001,
        "normalized rendering coordinates should be derived from the global location");

    const glm::dvec3 firstRenderOrigin{6370000.0, -250.0, 75.0};
    const glm::dvec3 secondRenderOrigin{-1000.0, 42000.0, 900.0};
    const glm::dvec3 firstRelative = globalPosition - firstRenderOrigin;
    const glm::dvec3 secondRelative = globalPosition - secondRenderOrigin;
    requireVectorNear(
        firstRenderOrigin + firstRelative,
        secondRenderOrigin + secondRelative,
        0.000000001,
        "render-origin rebasing should reconstruct the same global coordinate");
    wgen::tests::require(
        wgen::planetLocationPositionMeters(location, RADIUS) == globalPosition,
        "render-origin calculations must not mutate PlanetLocation global state");

    const wgen::PlanetLocation reanchored = wgen::reanchorPlanetLocation(
        location,
        {0.0, 0.0, 1.0},
        wgen::CubeSphereFace::Top);
    wgen::tests::require(
        reanchored.altitudeMeters == location.altitudeMeters &&
            reanchored.orientation == location.orientation &&
            reanchored.faceUv &&
            reanchored.faceUv->face == wgen::CubeSphereFace::Top,
        "re-anchoring should change global direction without losing navigation state");

    for (const glm::dvec3 up : std::array{
            glm::dvec3{0.0, 0.0, 1.0},
            glm::dvec3{0.0, 0.0, -1.0},
            glm::normalize(glm::dvec3{1.0, 2.0, 3.0})}) {
        const wgen::PlanetTangentFrame frame = wgen::planetTangentFrame(up);
        wgen::tests::require(
            std::abs(glm::dot(frame.east, frame.north)) <=
                    0.000000000000001 &&
                std::abs(glm::dot(frame.east, frame.up)) <=
                    0.000000000000001 &&
                std::abs(glm::dot(frame.north, frame.up)) <=
                    0.000000000000001,
            "planet tangent frames should remain orthogonal at poles and arbitrary anchors");
    }

    wgen::PlanetLocation invalidCache = location;
    invalidCache.faceUv = wgen::CubeSphereFaceUv{
        wgen::CubeSphereFace::Left,
        0.0,
        0.0,
    };
    wgen::tests::requireThrows<std::invalid_argument>(
        [&invalidCache] { wgen::validatePlanetLocation(invalidCache); },
        "planet location should reject a face cache that changes its global coordinate");
}

} // namespace

int main() {
    try {
        wgen::tests::runTest(
            "double projection and compatibility wrappers",
            testDoubleProjectionAndCompatibilityWrappers);
        wgen::tests::runTest(
            "direction face UV round trips",
            testDirectionFaceUvRoundTrips);
        wgen::tests::runTest(
            "every face seam and corner",
            testEveryFaceSeamAndCorner);
        wgen::tests::runTest(
            "maximum LOD coordinates remain distinct",
            testMaximumLodCoordinatesRemainDistinct);
        wgen::tests::runTest(
            "planet location and tangent frame",
            testPlanetLocationAndTangentFrame);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
