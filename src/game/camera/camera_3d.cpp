#include "camera_3d.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace lve {

namespace {

bool isFinite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

} // namespace

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
    setGlobalViewTarget(
        glm::dvec3{position},
        glm::dvec3{target},
        glm::dvec3{up});
}

void Camera3d::setGlobalViewTarget(
        glm::dvec3 position,
        glm::dvec3 target,
        glm::dvec3 up) {
    const glm::dvec3 targetOffset = target - position;
    if (!isFinite(position) || !isFinite(target) || !isFinite(up) ||
            glm::length(targetOffset) <= std::numeric_limits<double>::epsilon() ||
            glm::length(up) <= std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{"3D camera view vectors are invalid"};
    }

    const glm::dvec3 forward = glm::normalize(targetOffset);
    const glm::dvec3 right = glm::cross(forward, glm::normalize(up));
    if (glm::length(right) <= std::numeric_limits<double>::epsilon()) {
        throw std::invalid_argument{"3D camera up vector is parallel to its view direction"};
    }
    const glm::dvec3 normalizedRight = glm::normalize(right);
    const glm::dvec3 cameraUp = glm::cross(normalizedRight, forward);

    globalPosition_ = position;
    globalForward_ = forward;
    globalRight_ = normalizedRight;
    globalUp_ = cameraUp;
    position_ = glm::vec3{position};
    forward_ = glm::vec3{forward};
    right_ = glm::vec3{normalizedRight};
    up_ = glm::vec3{cameraUp};

    view_ = glm::mat4{1.0F};
    view_[0][0] = static_cast<float>(normalizedRight.x);
    view_[1][0] = static_cast<float>(normalizedRight.y);
    view_[2][0] = static_cast<float>(normalizedRight.z);
    view_[0][1] = static_cast<float>(cameraUp.x);
    view_[1][1] = static_cast<float>(cameraUp.y);
    view_[2][1] = static_cast<float>(cameraUp.z);
    view_[0][2] = static_cast<float>(forward.x);
    view_[1][2] = static_cast<float>(forward.y);
    view_[2][2] = static_cast<float>(forward.z);
    view_[3][0] = static_cast<float>(-glm::dot(normalizedRight, position));
    view_[3][1] = static_cast<float>(-glm::dot(cameraUp, position));
    view_[3][2] = static_cast<float>(-glm::dot(forward, position));
    updateRenderView();
}

void Camera3d::rebaseRenderOrigin(glm::dvec3 origin) {
    if (!isFinite(origin)) {
        throw std::invalid_argument{"3D camera render origin must be finite"};
    }
    renderOrigin_ = origin;
    updateRenderView();
}

glm::vec3 Camera3d::positionRelativeToRenderOrigin(
        glm::dvec3 globalPosition) const noexcept {
    return glm::vec3{globalPosition - renderOrigin_};
}

void Camera3d::updateRenderView() {
    renderView_ = glm::mat4{1.0F};
    renderView_[0][0] = static_cast<float>(globalRight_.x);
    renderView_[1][0] = static_cast<float>(globalRight_.y);
    renderView_[2][0] = static_cast<float>(globalRight_.z);
    renderView_[0][1] = static_cast<float>(globalUp_.x);
    renderView_[1][1] = static_cast<float>(globalUp_.y);
    renderView_[2][1] = static_cast<float>(globalUp_.z);
    renderView_[0][2] = static_cast<float>(globalForward_.x);
    renderView_[1][2] = static_cast<float>(globalForward_.y);
    renderView_[2][2] = static_cast<float>(globalForward_.z);

    const glm::dvec3 relativePosition = globalPosition_ - renderOrigin_;
    renderView_[3][0] = static_cast<float>(
        -glm::dot(globalRight_, relativePosition));
    renderView_[3][1] = static_cast<float>(
        -glm::dot(globalUp_, relativePosition));
    renderView_[3][2] = static_cast<float>(
        -glm::dot(globalForward_, relativePosition));
}

} // namespace lve
