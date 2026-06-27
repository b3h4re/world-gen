#pragma once

#include "game/3d/camera/camera_3d.hpp"
#include "game/input/input_state.hpp"

#include <glm/gtc/constants.hpp>

namespace lve {

class CameraController3d {
public:
    void update(const AppInputState &input, float frameTime, Camera3d &camera);

private:
    float yaw_{0.0F};
    float pitch_{glm::half_pi<float>()};
    float distance_{2.0F};
};

} // namespace lve
