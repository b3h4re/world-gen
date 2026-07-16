#pragma once

#include "game/camera/camera_2d.hpp"
#include "game/input/camera_controller_2d.hpp"
#include "game/camera/camera_3d.hpp"
#include "game/input/camera_controller_3d.hpp"
#include "game/input/camera_controller_planet.hpp"
#include "game/input/input_state.hpp"
#include "renderer/lve_frame_info.hpp"

#include <vector>

namespace lve {

struct CameraUpdateTarget {
    enum class Type {
        Camera2d,
        Camera3d
    };

    Type type;
    bool active{false};
    Camera2d *camera2d{nullptr};
    Camera3d *camera3d{nullptr};
};

CameraUpdateTarget makeCameraTarget(Camera2d &camera, bool active);
CameraUpdateTarget makeCameraTarget(Camera3d &camera, bool active);

class AppInputSystem {
public:
    void updateCameras(
        const AppInputState &input,
        float frameTime,
        float aspectRatio,
        double planetRadiusMeters,
        std::vector<std::pair<CameraUpdateTarget, TerrainRenderModes>> &targets);

    const wgen::PlanetLocation& planetLocation() const {
        return planetCameraController3d_.location();
    }

private:
    CameraController2d cameraController2d_{};
    CameraController3d planeMeshCameraController3d_{};
    CameraControllerPlanet planetCameraController3d_{};
};

} // namespace lve
