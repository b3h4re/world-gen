#include "camera_controller_2d.hpp"

namespace lve {

void CameraController2d::update(const AppInputState &input, float frameTime, Camera2d &camera) const {
    glm::vec2 direction{};
    direction.x += input.cameraMoveRight ? 1.0F : 0.0F;
    direction.x -= input.cameraMoveLeft ? 1.0F : 0.0F;
    direction.y += input.cameraMoveDown ? 1.0F : 0.0F;
    direction.y -= input.cameraMoveUp ? 1.0F : 0.0F;

    if (glm::dot(direction, direction) > 0.0F) {
        camera.move(glm::normalize(direction) * frameTime);
    }

    if (input.cameraZoomOut) {
        camera.zoom(1.0F + frameTime);
    }
    if (input.cameraZoomIn) {
        camera.zoom(1.0F / (1.0F + frameTime));
    }
}

} // namespace lve
