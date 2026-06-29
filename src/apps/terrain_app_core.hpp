#pragma once

#include "config/app_config.hpp"
#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"
#include "terrain/generators/generator.hpp"

#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <vector>

namespace lve {

struct TerrainMeshData {
    std::vector<Vertex2d> vertices2d;
    std::vector<std::uint32_t> indices2d;
    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
};

struct TerrainJobResult {
    wgen::HeightMap<float> heightMap;
    TerrainMeshData data;
};

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

class TerrainAppCore {
public:
    TerrainAppCore();
    explicit TerrainAppCore(const wgen::AppConfig& config);

    TerrainAppCore(const TerrainAppCore&) = delete;
    TerrainAppCore& operator=(const TerrainAppCore&) = delete;

    TerrainMeshData loadTerrain();
    void regenerateTerrain(std::uint32_t seed);
    void rotateColorFunction();
    std::optional<TerrainJobResult> tryTakeFinishedTerrainJob();

    const wgen::AppConfig& config() const { return config_; }

private:
    static void initGenerators(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        const wgen::TerrainConfig& terrainConfig);

    TerrainMeshData buildMeshData(const wgen::HeightMap<float>& heightMap);
    wgen::colorFromHeightFunc getActiveColorFunc() const;

    wgen::AppConfig config_{};
    std::size_t activeColorFuncId_{0};
    static constexpr std::size_t NUM_COLOR_FUNCTIONS = 2;
    static constexpr wgen::colorFromHeightFunc COLOR_FUNCTIONS[NUM_COLOR_FUNCTIONS] = {
        wgen::terrainColor,
        wgen::terrainBlackAndWhite
    };

    std::size_t usedGenerator_{0};
    std::vector<std::unique_ptr<wgen::Generator>> generators_{};
    wgen::HeightMap<float> activeHeightMap_{};

    std::future<TerrainMeshData> terrainReappendJob_{};
    std::future<TerrainJobResult> terrainGenerationJob_{};
    bool terrainJobRunning_{false};
    std::mutex generatorsMutex_{};
};

} // namespace lve
