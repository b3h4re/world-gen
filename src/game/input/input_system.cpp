#include "input_system.hpp"

#include <glm/glm.hpp>

namespace lve {

CameraUpdateTarget makeCameraTarget(Camera2d &camera, bool active) {
    return {
        CameraUpdateTarget::Type::Camera2d,
        active,
        &camera,
        nullptr,
    };
}

CameraUpdateTarget makeCameraTarget(Camera3d &camera, bool active) {
    return {
        CameraUpdateTarget::Type::Camera3d,
        active,
        nullptr,
        &camera,
    };
}

void AppInputSystem::updateCameras(
    const AppInputState &input,
    float frameTime,
    float aspectRatio,
    std::vector<CameraUpdateTarget> &targets) {
    for (auto &target : targets) {
        switch (target.type) {
            case CameraUpdateTarget::Type::Camera2d:
                target.camera2d->setAspectRatio(aspectRatio);
                if (target.active) {
                    cameraController2d_.update(input, frameTime, *target.camera2d);
                }
                break;
            case CameraUpdateTarget::Type::Camera3d:
                target.camera3d->setPerspectiveProjection(glm::radians(50.0F), aspectRatio, 0.1F, 20.0F);
                if (target.active) {
                    cameraController3d_.update(input, frameTime, *target.camera3d);
                }
                break;
        }
    }
}

} // namespace lve
