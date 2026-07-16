#pragma once

#include "game/camera/camera_3d.hpp"
#include "game/input/input_state.hpp"
#include "game/input/planet_navigation.hpp"

namespace lve {

class CameraControllerPlanet {
public:
    void update(
        const AppInputState& input,
        float frameTime,
        double planetRadiusMeters,
        Camera3d& camera);

    const wgen::PlanetLocation& location() const {
        return navigationState_.location;
    }
    const PlanetNavigationState& navigationState() const {
        return navigationState_;
    }

private:
    PlanetNavigationState navigationState_{};
    bool locationInitialized_{false};
};

} // namespace lve
