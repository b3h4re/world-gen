#include "camera_3d.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace lve {

void Camera3d::setPerspectiveProjection(float fovy, float aspectRatio, float near, float far) {
    if (!std::isfinite(fovy) || fovy <= 0.0F || fovy >= std::numbers::pi_v<float> ||
            !std::isfinite(aspectRatio) || aspectRatio <= 0.0F ||
            !std::isfinite(near) || near <= 0.0F ||
            !std::isfinite(far) || far <= near) {
        throw std::invalid_argument{"3D camera perspective parameters are invalid"};
    }

    verticalFov_ = fovy;
    aspectRatio_ = aspectRatio;
    nearPlane_ = near;
    farPlane_ = far;
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
    const auto isFinite = [](glm::vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const glm::vec3 targetOffset = target - position;
    if (!isFinite(position) || !isFinite(target) || !isFinite(up) ||
            glm::length(targetOffset) <= std::numeric_limits<float>::epsilon() ||
            glm::length(up) <= std::numeric_limits<float>::epsilon()) {
        throw std::invalid_argument{"3D camera view vectors are invalid"};
    }

    const glm::vec3 forward = glm::normalize(targetOffset);
    const glm::vec3 right = glm::cross(forward, glm::normalize(up));
    if (glm::length(right) <= std::numeric_limits<float>::epsilon()) {
        throw std::invalid_argument{"3D camera up vector is parallel to its view direction"};
    }
    const glm::vec3 normalizedRight = glm::normalize(right);
    const glm::vec3 cameraUp = glm::cross(normalizedRight, forward);

    position_ = position;
    forward_ = forward;
    right_ = normalizedRight;
    up_ = cameraUp;

    view_ = glm::mat4{1.0F};
    view_[0][0] = normalizedRight.x;
    view_[1][0] = normalizedRight.y;
    view_[2][0] = normalizedRight.z;
    view_[0][1] = cameraUp.x;
    view_[1][1] = cameraUp.y;
    view_[2][1] = cameraUp.z;
    view_[0][2] = forward.x;
    view_[1][2] = forward.y;
    view_[2][2] = forward.z;
    view_[3][0] = -glm::dot(normalizedRight, position);
    view_[3][1] = -glm::dot(cameraUp, position);
    view_[3][2] = -glm::dot(forward, position);
}

} // namespace lve
