#pragma once

#include "game/3d/camera/camera_3d.hpp"

struct GLFWwindow;

namespace lve {

class CameraController3d {
public:
    void update(GLFWwindow *window, float frameTime, Camera3d &camera);

private:
    float yaw_{0.0F};
    float pitch_{0.75F};
    float distance_{3.0F};
};

} // namespace lve
