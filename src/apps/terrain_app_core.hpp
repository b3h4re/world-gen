#pragma once

#include "config/app_config.hpp"
#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"
#include "renderer/compute/gpu_terrain_pipeline.hpp"
#include "terrain/generators/generator.hpp"
#include "terrain/generators/generator_spec.hpp"
#include "utils/thread_pool.hpp"
#include "utils/color_map.hpp"
#include "terrain/planet.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace lve {

struct TerrainMeshData {
    std::vector<Vertex2d> vertices2d;
    std::vector<std::uint32_t> indices2d;
    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
    std::vector<Vertex3d> verticesPlanet;
    std::vector<std::uint32_t> indicesPlanet;

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
    std::vector<std::uint32_t>& indices);

void appendHeightMapMesh3d(
    const wgen::HeightMap<float>& heightMap,
    std::vector<Vertex3d>& vertices,
    std::vector<std::uint32_t>& indices);

void appendPlanetmesh(
    const wgen::Planet<float>& planet,
    std::vector<Vertex3d>& vertices,
    std::vector<std::uint32_t>& indices
);

class TerrainAppCore {
public:
    TerrainAppCore();
    explicit TerrainAppCore(const wgen::AppConfig& config);

    TerrainAppCore(const TerrainAppCore&) = delete;
    TerrainAppCore& operator=(const TerrainAppCore&) = delete;

    TerrainMeshData loadTerrain();
    void regenerateTerrain(wgen::SeedType seed);
    void rotateColorFunction();
    void setPipeline(wgen::GeneratorPipelineSpec pipeline);
    wgen::GeneratorPipelineSpec currentPipeline() const;
    std::optional<TerrainJobResult> tryTakeFinishedTerrainJob();

    const wgen::AppConfig& config() const { return config_; }

    wgen::HeightMap<float>& activeHeightMap() { return activeHeightMap_; }

    ColorFunctions getActiveColorFuncID() const { return activeColorFuncId_; }

private:
    static void initGenerators(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        const wgen::GeneratorPipelineSpec& pipelineSpec,
        wgen::SeedType seed);
    static void setGeneratorSeeds(
        std::vector<std::unique_ptr<wgen::Generator>>& generators,
        wgen::SeedType seed);
    static std::vector<wgen::SeedType> generatorSeeds(std::size_t count, wgen::SeedType seed);
    static wgen::GeneratorPipelineSpec defaultPipelineSpec(const wgen::TerrainConfig& terrainConfig);
    wgen::SeedType activeSeed() const;

    wgen::HeightMap<float> generateHeightMap(const wgen::TerrainConfig& terrainConfig);
    wgen::HeightMap<float> generateHeightMapCpu(std::size_t generatorIndex, const wgen::TerrainConfig& terrainConfig);
    wgen::HeightMap<float> generateHeightMapGpu(
        const std::vector<GpuGeneratorRequest>& requests,
        const wgen::TerrainConfig& terrainConfig);

    TerrainMeshData buildMeshData(const wgen::HeightMap<float>& heightMap);
    std::function<glm::vec3(float)> getActiveColorFunc() const;

    wgen::AppConfig config_{};
    wgen::GeneratorPipelineSpec pipelineSpec_{};
    ColorFunctions activeColorFuncId_{ColorFunctions::TerrainColorStandard};

    std::size_t usedGenerator_{0};
    std::vector<std::unique_ptr<wgen::Generator>> generators_{};
    std::unique_ptr<GpuTerrainPipeline> gpuPipeline_{};
    wgen::ThreadPool threadPool_{};
    wgen::HeightMap<float> activeHeightMap_{};

    std::future<TerrainJobResult> terrainGenerationJob_{};
    bool terrainJobRunning_{false};
    std::mutex generatorsMutex_{};
};

} // namespace lve
