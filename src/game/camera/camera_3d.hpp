#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace lve {

class Camera3d {
public:
    void setPerspectiveProjection(float fovy, float aspectRatio, float near, float far);
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = {0.0F, 1.0F, 0.0F});

    const glm::mat4 &projection() const { return projection_; }
    const glm::mat4 &view() const { return view_; }
    glm::mat4 projectionView() const { return projection_ * view_; }

private:
    glm::mat4 projection_{1.0F};
    glm::mat4 view_{1.0F};
};

} // namespace lve
