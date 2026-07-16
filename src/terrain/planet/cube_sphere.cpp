#include "cube_sphere.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace wgen {


std::size_t faceID(CubeSphereFace face) {
    switch (face) {
        case CubeSphereFace::Top:    { return 0; }
        case CubeSphereFace::Bottom: { return 1; }
        case CubeSphereFace::Right:  { return 2; }
        case CubeSphereFace::Left:   { return 3; }
        case CubeSphereFace::Back:   { return 4; }
        case CubeSphereFace::Front:  { return 5; }
    }
    throw std::invalid_argument("Unknown cube face appeared in the faceID method");
}


namespace {

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 cubeFacePoint(CubeSphereFace face, double u, double v) {
    static_cast<void>(faceID(face));
    switch (face) {
        case CubeSphereFace::Top:    { return { u, v, 1.0}; }
        case CubeSphereFace::Bottom: { return {-u, v, -1.0}; }
        case CubeSphereFace::Right:  { return {1.0, v, -u}; }
        case CubeSphereFace::Left:   { return {-1.0, v, u}; }
        case CubeSphereFace::Back:   { return {u, -1.0, v}; }
        case CubeSphereFace::Front:  { return {u, 1.0, -v}; }
    }
    throw std::invalid_argument{"cube sphere face is invalid"};
}

glm::dvec2 initialFaceUv(CubeSphereFace face, glm::dvec3 direction) {
    constexpr double MIN_DENOMINATOR = 0.000000000001;
    const auto divide = [](double numerator, double denominator) {
        return std::abs(denominator) > MIN_DENOMINATOR
            ? numerator / denominator
            : 0.0;
    };
    glm::dvec2 result;
    switch (face) {
        case CubeSphereFace::Top:
            result = {divide(direction.x, direction.z), divide(direction.y, direction.z)};
            break;
        case CubeSphereFace::Bottom:
            result = {divide(direction.x, direction.z), divide(-direction.y, direction.z)};
            break;
        case CubeSphereFace::Right:
            result = {divide(-direction.z, direction.x), divide(direction.y, direction.x)};
            break;
        case CubeSphereFace::Left:
            result = {divide(-direction.z, direction.x), divide(-direction.y, direction.x)};
            break;
        case CubeSphereFace::Back:
            result = {divide(-direction.x, direction.y), divide(-direction.z, direction.y)};
            break;
        case CubeSphereFace::Front:
            result = {divide(direction.x, direction.y), divide(-direction.z, direction.y)};
            break;
    }
    return glm::clamp(result, glm::dvec2{-1.0}, glm::dvec2{1.0});
}

struct FaceUvCandidate {
    CubeSphereFaceUv faceUv{};
    double squaredError{std::numeric_limits<double>::infinity()};
};

FaceUvCandidate solveFaceUv(CubeSphereFace face, glm::dvec3 direction) {
    glm::dvec2 uv = initialFaceUv(face, direction);
    constexpr double DERIVATIVE_STEP = 0.000001;
    constexpr double MIN_DETERMINANT = 1e-18;
    for (std::size_t iteration = 0; iteration < 20; ++iteration) {
        const glm::dvec3 projected = cubeSphereDirection(face, uv.x, uv.y);
        const glm::dvec3 residual = projected - direction;
        if (glm::dot(residual, residual) <= 1e-30) {
            break;
        }

        const auto derivative = [face, &uv](bool uAxis) {
            glm::dvec2 lower = uv;
            glm::dvec2 upper = uv;
            const std::size_t axis = uAxis ? 0 : 1;
            lower[axis] = std::max(-1.0, lower[axis] - DERIVATIVE_STEP);
            upper[axis] = std::min(1.0, upper[axis] + DERIVATIVE_STEP);
            const double span = upper[axis] - lower[axis];
            if (span <= std::numeric_limits<double>::epsilon()) {
                return glm::dvec3{};
            }
            return (cubeSphereDirection(face, upper.x, upper.y) -
                cubeSphereDirection(face, lower.x, lower.y)) / span;
        };
        const glm::dvec3 derivativeU = derivative(true);
        const glm::dvec3 derivativeV = derivative(false);
        const double uu = glm::dot(derivativeU, derivativeU);
        const double uvTerm = glm::dot(derivativeU, derivativeV);
        const double vv = glm::dot(derivativeV, derivativeV);
        const double determinant = uu * vv - uvTerm * uvTerm;
        if (determinant <= MIN_DETERMINANT) {
            break;
        }
        const double uResidual = glm::dot(derivativeU, residual);
        const double vResidual = glm::dot(derivativeV, residual);
        const glm::dvec2 delta{
            (vv * uResidual - uvTerm * vResidual) / determinant,
            (uu * vResidual - uvTerm * uResidual) / determinant,
        };
        uv = glm::clamp(uv - delta, glm::dvec2{-1.0}, glm::dvec2{1.0});
        if (glm::dot(delta, delta) <= 1e-30) {
            break;
        }
    }

    const glm::dvec3 residual = cubeSphereDirection(face, uv.x, uv.y) - direction;
    return {{face, uv.x, uv.y}, glm::dot(residual, residual)};
}

} // namespace

glm::dvec3 spherifyCube(glm::dvec3 p) {
    if (!isFinite(p)) {
        throw std::invalid_argument{"cube projection point must be finite"};
    }
    const glm::dvec3 squared = p * p;
    const glm::dvec3 scale{
        std::sqrt(std::max(0.0, 1.0 - squared.y / 2.0 - squared.z / 2.0 + squared.y * squared.z / 3.0)),
        std::sqrt(std::max(0.0, 1.0 - squared.z / 2.0 - squared.x / 2.0 + squared.z * squared.x / 3.0)),
        std::sqrt(std::max(0.0, 1.0 - squared.x / 2.0 - squared.y / 2.0 + squared.x * squared.y / 3.0)),
    };
    return p * scale;
}

glm::dvec3 cubeSphereDirection(CubeSphereFace face, double u, double v) {
    if (!std::isfinite(u) || !std::isfinite(v) ||
            u < -1.0 || u > 1.0 || v < -1.0 || v > 1.0) {
        throw std::invalid_argument{"cube sphere UV must be finite and inside the face"};
    }
    return spherifyCube(cubeFacePoint(face, u, v));
}

CubeSphereFaceUv directionToCubeSphereFaceUv(
        glm::dvec3 direction,
        std::optional<CubeSphereFace> preferredFace) {
    if (!isFinite(direction) ||
            glm::length(direction) <= std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{"cube sphere direction must be finite and non-zero"};
    }
    direction = glm::normalize(direction);

    if (preferredFace) {
        static_cast<void>(faceID(*preferredFace));
        const FaceUvCandidate preferred = solveFaceUv(*preferredFace, direction);
        if (preferred.squaredError <= 1e-24) {
            return preferred.faceUv;
        }
    }

    FaceUvCandidate best;
    for (const CubeSphereFace face : FACES) {
        const FaceUvCandidate candidate = solveFaceUv(face, direction);
        if (candidate.squaredError < best.squaredError) {
            best = candidate;
        }
    }
    if (best.squaredError > 1e-24) {
        throw std::runtime_error{"cube sphere direction could not be mapped to a face"};
    }
    return best.faceUv;
}

glm::vec3 spherifyCube(glm::vec3 p) {
    return glm::vec3{spherifyCube(glm::dvec3{p})};
}

glm::vec3 cubeSphereDirection(CubeSphereFace face, float u, float v) {
    return glm::vec3{cubeSphereDirection(
        face,
        static_cast<double>(u),
        static_cast<double>(v))};
}

}
