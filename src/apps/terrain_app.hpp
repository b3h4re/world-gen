#pragma once

#include "device/lve_device.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "renderer/lve_renderer.hpp"
#include "window/lve_window.hpp"

#include <vector>

namespace lve {

class TerrainApp {
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    TerrainApp();

    TerrainApp(const TerrainApp &) = delete;
    TerrainApp &operator=(const TerrainApp &) = delete;

    void run();

private:
    void loadTerrain();

    LveWindow window_{WIDTH, HEIGHT, "World Generator"};
    LveDevice device_{window_};
    LveRenderer renderer_{window_, device_};
    std::vector<GameObject2d> objects_;
};

} // namespace lve
