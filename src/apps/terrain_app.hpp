#pragma once

#include "config/app_config.hpp"
#include "device/lve_device.hpp"
#include "game/ui/dropdown_menu.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "renderer/lve_renderer.hpp"
#include "stb/font_atlas.hpp"
#include "terrain/generators/generator.hpp"
#include "window/lve_window.hpp"

#include <vector>

namespace lve {

void appendHeightMapMesh(
    const wgen::HeightMap<float>& heightMap,
    float minX,
    float maxX,
    float minY,
    float maxY,
    std::vector<Vertex2d>& vertices,
    std::vector<std::uint32_t>& indices,
    wgen::colorFromHeightFunc colorFunc);

void appendHeightMapMesh3d(
    const wgen::HeightMap<float>& heightMap,
    std::vector<Vertex3d>& vertices,
    std::vector<std::uint32_t>& indices,
    wgen::colorFromHeightFunc colorFunc);

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
    void reloadObjects();
    void regenerateTerrain(std::uint32_t seed);
    void initGenerators(const wgen::TerrainConfig& terrainConfig);
    void initDropDownMenu();
    void initFontFamily();
    void initDescriptorPool();
    void rotateColorFunction();

    wgen::colorFromHeightFunc getActiveColorFunc();
    std::size_t activeColorFuncId{0};
    constexpr static std::size_t NUM_COLOR_FUNCTIONS = 2;
    constexpr static wgen::colorFromHeightFunc COLOR_FUNCTIONS[NUM_COLOR_FUNCTIONS] = {
        wgen::terrainColor,
        wgen::terrainBlackAndWhite
    };

    std::size_t used_generator = 0;
    std::vector<std::unique_ptr<wgen::Generator>> generators;
    bool render3d_{false};

    wgen::HeightMap<float> activeHeghtMap;

    LveWindow window_{WIDTH, HEIGHT, "World Generator"};
    LveDevice device_{window_};
    LveRenderer renderer_{window_, device_};
    std::vector<GameObject2d> objects2d_;
    FontFamily fontFamily_{};
    std::unique_ptr<DropdownMenu> dropdownMenu_;
    std::vector<GameObject3d> objects3d_;

    std::unique_ptr<LveDescriptorPool> globalPool_{};
};

} // namespace lve
