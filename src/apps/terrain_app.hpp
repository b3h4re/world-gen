#pragma once

#include "device/lve_device.hpp"
#include "game/2d/objects/game_object_2d.hpp"
#include "game/3d/objects/game_object_3d.hpp"
#include "game/ui/dropdown_menu.hpp"
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
    void regenerateTerrain(std::uint32_t seed);
    void initGenerators(const wgen::TerrainConfig& terrainConfig);
    void initDropDownMenu();
    std::size_t used_generator = 0;
    std::vector<std::unique_ptr<wgen::Generator>> generators;
    bool render3d_{false};

    LveWindow window_{WIDTH, HEIGHT, "World Generator"};
    LveDevice device_{window_};
    LveRenderer renderer_{window_, device_};
    std::vector<GameObject2d> objects2d_;
    std::unique_ptr<DropdownMenu> dropdownMenu_;
    std::vector<GameObject3d> objects3d_;
};

} // namespace lve
