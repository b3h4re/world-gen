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
#include <mutex>
#include <future>

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


struct RetiredFrameObjects {
    std::vector<GameObject2d> objects2d;
    std::vector<GameObject3d> objects3d;

    void clear() {
        objects2d.clear();
        objects3d.clear();
    }
};

struct TerrainMeshData {
    std::vector<Vertex2d> vertices2d;
    std::vector<std::uint32_t> indices2d;
    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
};

struct TerrainJobResult {
    std::vector<std::unique_ptr<wgen::Generator>> generators;
    wgen::HeightMap<float> heightMap;
    TerrainMeshData data;
};


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
    static void initGenerators(std::vector<std::unique_ptr<wgen::Generator>>& generators, const wgen::TerrainConfig& terrainConfig);
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
    FontFamily fontFamily_{};
    std::unique_ptr<DropdownMenu> dropdownMenu_;

    std::vector<GameObject2d> objects2d_;
    std::vector<GameObject3d> objects3d_;
    bool pendingObjectReload_{false};

    std::array<RetiredFrameObjects, LveSwapChain::MAX_FRAMES_IN_FLIGHT> retiredObjects_{};

    std::unique_ptr<LveDescriptorPool> globalPool_{};

    std::mutex terrainJobMutex_;
    std::future<TerrainMeshData> terrainReappendJob_;;
    std::uint64_t terrainJobVersion_{0};
    std::uint64_t latestAcceptedTerrainJobVersion_{0};
    bool terrainJobRunning_{false};
    void tryApplyFinishedTerrainJob(int frameIndex);

    std::future<TerrainJobResult> terrainGenerationJob_;
    std::uint64_t terrainBuildVersion_{0};



};

} // namespace lve
