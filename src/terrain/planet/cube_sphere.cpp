#include "cube_sphere.hpp"

#include <algorithm>
#include <cmath>
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


glm::vec3 spherifyCube(glm::vec3 p) {
    const glm::vec3 squared = p * p;
    const glm::vec3 scale{
        std::sqrt(std::max(0.0F, 1.0F - squared.y / 2.0F - squared.z / 2.0F + squared.y * squared.z / 3.0F)),
        std::sqrt(std::max(0.0F, 1.0F - squared.z / 2.0F - squared.x / 2.0F + squared.z * squared.x / 3.0F)),
        std::sqrt(std::max(0.0F, 1.0F - squared.x / 2.0F - squared.y / 2.0F + squared.x * squared.y / 3.0F)),
    };
    return p * scale;
}


glm::vec3 cubeSphereDirection(CubeSphereFace face, float u, float v) {
    glm::vec3 p;
    switch (face) {
        case CubeSphereFace::Top:    { p = {  u,  v,  1 }; break; }
        case CubeSphereFace::Bottom: { p = { -u,  v, -1 }; break; }
        case CubeSphereFace::Right:  { p = {  1,  v, -u }; break; }
        case CubeSphereFace::Left:   { p = { -1,  v,  u }; break; }
        case CubeSphereFace::Back:   { p = {  u, -1,  v }; break; }
        case CubeSphereFace::Front:  { p = {  u,  1, -v }; break; }
    }
    return spherifyCube(p);
}

}
