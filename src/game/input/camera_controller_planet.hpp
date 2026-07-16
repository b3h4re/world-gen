#pragma once

#include "game/camera/camera_3d.hpp"
#include "game/input/input_state.hpp"
#include "terrain/planet/planet_location.hpp"

#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace lve {

class CameraControllerPlanet {
public:
    void update(
        const AppInputState& input,
        float frameTime,
        double planetRadiusMeters,
        Camera3d& camera);

    const wgen::PlanetLocation& location() const { return location_; }

private:
    inline const static glm::dvec3 startingUp{1.0, 0.0, 0.0};
    inline const static glm::dvec3 startingPosition{0.0, 0.0, 1.0};

    glm::dquat orbitRotation_{1.0, 0.0, 0.0, 0.0};
    wgen::PlanetLocation location_{};
    bool locationInitialized_{false};
};

} // namespace lve
