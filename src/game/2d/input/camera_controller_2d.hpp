#pragma once

#include "game/2d/camera/camera_2d.hpp"

struct GLFWwindow;

namespace lve {

class CameraController2d {
public:
    void update(GLFWwindow *window, float frameTime, Camera2d &camera) const;
};

} // namespace lve
