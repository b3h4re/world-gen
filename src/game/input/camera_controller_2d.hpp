#pragma once

#include "game/2d/camera/camera_2d.hpp"
#include "game/input/input_state.hpp"

namespace lve {

class CameraController2d {
public:
    void update(const AppInputState &input, float frameTime, Camera2d &camera) const;
};

} // namespace lve
