#include "cube_sphere.hpp"



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
    return glm::normalize(p);
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
