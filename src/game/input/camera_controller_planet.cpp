#include "camera_controller_planet.hpp"

#include <iostream>


#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace lve {

void CameraControllerPlanet::update(const AppInputState &input, float frameTime, Camera3d &camera) {
    constexpr float rotateSpeed = 1.5F;
    constexpr float zoomSpeed = 2.0F;

	float deltaYaw{0.0f};
	float deltaPitch{0.0f};
    deltaYaw += input.cameraMoveRight ? rotateSpeed * frameTime : 0.0F;
    deltaYaw -= input.cameraMoveLeft ? rotateSpeed * frameTime : 0.0F;
    deltaPitch += input.cameraMoveUp ? rotateSpeed * frameTime : 0.0F;
    deltaPitch -= input.cameraMoveDown ? rotateSpeed * frameTime : 0.0F;
    distance_ += input.cameraZoomOut ? zoomSpeed * frameTime : 0.0F;
    distance_ -= input.cameraZoomIn ? zoomSpeed * frameTime : 0.0F;

    distance_ = std::clamp(distance_, 1.2F, 8.0F);

	glm::quat rotUp{
		glm::cos(deltaPitch / 2.0f),
		0.0f,
		glm::sin(deltaPitch / 2.0f),
		0.0f
	};
	glm::quat rotSide{
		glm::cos(deltaYaw / 2.0f),
		glm::sin(deltaYaw / 2.0f),
		0.0f,
		0.0f
	};

	orbitRotation_ = glm::normalize(orbitRotation_ * rotSide * rotUp);

	glm::quat positionQ = orbitRotation_ * glm::quat{0.0f, startingPos} * glm::inverse(orbitRotation_);
	glm::quat upQ = orbitRotation_ * glm::quat{0.0f, startingUp} * glm::inverse(orbitRotation_);

	glm::vec3 position = distance_* glm::vec3{positionQ.y, positionQ.z, positionQ.x};
	glm::vec3 up{upQ.y, upQ.z, upQ.x};

    const glm::vec3 target{0.0F, 0.0F, 0.0F};

	camera.setViewTarget(position, target, up);
}

} // namespace lve
