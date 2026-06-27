#include "camera_3d.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace lve {

void Camera3d::setPerspectiveProjection(float fovy, float aspectRatio, float near, float far) {
    assert(std::abs(aspectRatio) > std::numeric_limits<float>::epsilon());
    const float tanHalfFovy = std::tan(fovy / 2.0F);
    projection_ = glm::mat4{0.0F};
    projection_[0][0] = 1.0F / (aspectRatio * tanHalfFovy);
    projection_[1][1] = 1.0F / tanHalfFovy;
    projection_[1][1] *= -1.0F;
    projection_[2][2] = far / (far - near);
    projection_[2][3] = 1.0F;
    projection_[3][2] = -(far * near) / (far - near);
}

void Camera3d::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
    const glm::vec3 forward = glm::normalize(target - position);
    const glm::vec3 right = glm::normalize(glm::cross(forward, up));
    const glm::vec3 cameraUp = glm::cross(right, forward);

    view_ = glm::mat4{1.0F};
    view_[0][0] = right.x;
    view_[1][0] = right.y;
    view_[2][0] = right.z;
    view_[0][1] = cameraUp.x;
    view_[1][1] = cameraUp.y;
    view_[2][1] = cameraUp.z;
    view_[0][2] = forward.x;
    view_[1][2] = forward.y;
    view_[2][2] = forward.z;
    view_[3][0] = -glm::dot(right, position);
    view_[3][1] = -glm::dot(cameraUp, position);
    view_[3][2] = -glm::dot(forward, position);
}

} // namespace lve
