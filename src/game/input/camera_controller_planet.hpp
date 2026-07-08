#pragma once

#include "game/camera/camera_3d.hpp"
#include "game/input/input_state.hpp"

#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace lve {

class CameraControllerPlanet {
public:
    void update(const AppInputState &input, float frameTime, Camera3d &camera);

private:
    inline const static glm::vec3 startingUp = glm::vec3{1.0f, 0.0f, 0.0f};
    inline const static glm::vec3 startingPos = glm::vec3{0.0f, 0.0f, 1.0f};

    glm::quat orbitRotation_{1.0f, 0.0f, 0.0f, 0.0f};
    float distance_{2.0F};
};

} // namespace lve
