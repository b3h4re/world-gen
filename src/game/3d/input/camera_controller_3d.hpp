#pragma once

#include "game/3d/camera/camera_3d.hpp"
#include "game/input/input_state.hpp"

namespace lve {

class CameraController3d {
public:
    void update(const AppInputState &input, float frameTime, Camera3d &camera);

private:
    float yaw_{0.0F};
    float pitch_{0.75F};
    float distance_{3.0F};
};

} // namespace lve
