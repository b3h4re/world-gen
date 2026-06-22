#pragma once

#include "device/lve_device.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "renderer/lve_renderer.hpp"
#include "window/lve_window.hpp"
#include "terrain/generators/generator.hpp"
#include "config/app_config.hpp"

#include <vector>

namespace lve {

class TerrainApp {
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    TerrainApp();
    TerrainApp(const wgen::AppConfig &config);

    TerrainApp(const TerrainApp &) = delete;
    TerrainApp &operator=(const TerrainApp &) = delete;

    void run();

private:
    wgen::AppConfig config;

    void loadTerrain();
    std::size_t used_generator = 0;
    std::vector<std::unique_ptr<wgen::Generator>> generators;

    LveWindow window_{WIDTH, HEIGHT, "World Generator"};
    LveDevice device_{window_};
    LveRenderer renderer_{window_, device_};
    std::vector<GameObject2d> objects_;
};

} // namespace lve
