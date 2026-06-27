#include "camera_controller_3d.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace lve {

void CameraController3d::update(const AppInputState &input, float frameTime, Camera3d &camera) {
    constexpr float rotateSpeed = 1.5F;
    constexpr float zoomSpeed = 2.0F;

    yaw_ += input.cameraMoveRight ? rotateSpeed * frameTime : 0.0F;
    yaw_ -= input.cameraMoveLeft ? rotateSpeed * frameTime : 0.0F;
    pitch_ += input.cameraMoveUp ? rotateSpeed * frameTime : 0.0F;
    pitch_ -= input.cameraMoveDown ? rotateSpeed * frameTime : 0.0F;
    distance_ += input.cameraZoomOut ? zoomSpeed * frameTime : 0.0F;
    distance_ -= input.cameraZoomIn ? zoomSpeed * frameTime : 0.0F;

    distance_ = std::clamp(distance_, 1.2F, 8.0F);

    const glm::vec3 target{0.0F, 0.0F, 0.0F};
    const glm::vec3 position{
        distance_ * std::cos(pitch_) * std::sin(yaw_),
        distance_ * std::sin(pitch_),
        distance_ * std::cos(pitch_) * std::cos(yaw_)
    };
    const glm::vec3 up = std::abs(std::cos(pitch_)) < 0.001F
        ? glm::vec3{0.0F, 0.0F, -1.0F}
        : glm::vec3{0.0F, 1.0F, 0.0F};
    camera.setViewTarget(position, target, up);
}

} // namespace lve
