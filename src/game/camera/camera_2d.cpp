#include "camera_2d.hpp"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace lve {

void Camera2d::setAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
    updateProjection();
}

void Camera2d::setPosition(glm::vec2 position) {
    position_ = position;
    updateProjection();
}

void Camera2d::setZoom(float zoom) {
    zoom_ = std::clamp(zoom, 0.2F, 8.0F);
    updateProjection();
}

void Camera2d::move(glm::vec2 offset) {
    position_ += offset;
    updateProjection();
}

void Camera2d::zoom(float factor) {
    zoom_ = std::clamp(zoom_ * factor, 0.2F, 8.0F);
    updateProjection();
}

void Camera2d::updateProjection() {
    const float halfHeight = zoom_;
    const float halfWidth = aspectRatio_ * halfHeight;
    projectionView_ = glm::ortho(
        position_.x - halfWidth,
        position_.x + halfWidth,
        position_.y - halfHeight,
        position_.y + halfHeight,
        0.0F,
        1.0F);
    projectionView_[1][1] *= -1.0F;
}

} // namespace lve
