#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace lve {

class Camera2d {
public:
    void setAspectRatio(float aspectRatio);
    void move(glm::vec2 offset);
    void zoom(float factor);

    const glm::mat4 &projectionView() const { return projectionView_; }

private:
    void updateProjection();

    float aspectRatio_{1.0F};
    float zoom_{1.0F};
    glm::vec2 position_{};
    glm::mat4 projectionView_{1.0F};
};

} // namespace lve
