#pragma once

#include "config/app_config.hpp"
#include "ui/gui_helpers.hpp"
#include "renderer/objects/mesh_2d.hpp"
#include "renderer/objects/mesh_3d.hpp"
#include "renderer/compute/gpu_terrain_pipeline.hpp"
#include "terrain/generators/2d/generator.hpp"
#include "terrain/generators/2d/generator_spec.hpp"
#include "terrain/generators/3d/generator_spec.hpp"
#include "terrain/planet/planet_lod_coordinator.hpp"
#include "terrain/planet/planet_patch_mesh.hpp"
#include "terrain/planet/terrain_field.hpp"
#include "utils/thread_pool.hpp"
#include "utils/color_map.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace lve {

struct TerrainPlaneMeshData {
    std::vector<Vertex2d> vertices2d;
    std::vector<std::uint32_t> indices2d;
    std::vector<Vertex3d> vertices3d;
    std::vector<std::uint32_t> indices3d;
};

struct TerrainJobResult {
    wgen::HeightMap<float> heightMap;
    wgen::TerrainFieldSnapshot terrainField;
    std::uint64_t terrainEpoch{};
    std::optional<TerrainPlaneMeshData> terrainMesh;
    std::optional<PlanetPatchMeshBatch> planetBatch;
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

class TerrainAppCore {
public:
    TerrainAppCore();
    explicit TerrainAppCore(const wgen::AppConfig& config);

    TerrainAppCore(const TerrainAppCore&) = delete;
    TerrainAppCore& operator=(const TerrainAppCore&) = delete;

    void regenerateTerrain(wgen::SeedType seed, TerrainGenerationTarget target = TerrainGenerationTarget::All);
    void rotateColorFunction();
    void setPipeline(wgen::GeneratorPipelineSpec pipeline);
    void setPlanetPipeline(wgen::Generator3dPipelineSpec pipeline);
    void setPlanetShape(std::size_t resolution, float radius);
    void setMaximumPlanetPatchLevel(std::uint8_t level);
    void updatePlanetLod(const wgen::PlanetLodView& view, double deltaSeconds);
    void setPlanetLodTransitionTimeScale(double timeScale);
    wgen::GeneratorPipelineSpec currentPipeline() const;
    wgen::Generator3dPipelineSpec currentPlanetPipeline() const;
    wgen::TerrainDisplayHeightRange activePlanetDisplayHeightRange() const;
    std::optional<TerrainJobResult> tryTakeFinishedTerrainJob();
    bool isTerrainJobRunning() const { return terrainJobRunning_; }
    bool isBlockingTerrainJobRunning() const;
    const std::vector<wgen::PlanetPatchDrawState>& planetDrawPatchStates() const {
        return planetLodCoordinator_.visibleDrawStates();
    }

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
    static wgen::Generator3dPipelineSpec defaultPlanetPipelineSpec(const wgen::PlanetConfig& planetConfig);
    wgen::SeedType activeSeed() const;

    wgen::HeightMap<float> generateHeightMap(const wgen::TerrainConfig& terrainConfig);
    wgen::HeightMap<float> generateHeightMapCpu(std::size_t generatorIndex, const wgen::TerrainConfig& terrainConfig);
    wgen::HeightMap<float> generateHeightMapGpu(
        const std::vector<GpuGeneratorRequest>& requests,
        const wgen::TerrainConfig& terrainConfig);
    static TerrainPlaneMeshData buildPlaneMeshData(const wgen::HeightMap<float>& heightMap);
    static PlanetPatchMeshBatch buildPlanetPatchBatch(
        const wgen::TerrainFieldSnapshot& terrainField,
        const PlanetPatchVersion& version,
        const std::vector<wgen::PlanetPatchId>& upsertIds,
        const std::vector<wgen::PlanetPatchId>& removalIds,
        float skirtDepthMultiplier);
    static wgen::PlanetLodSurface buildPlanetLodSurface(
        const wgen::TerrainField& terrainField,
        std::uint32_t patchQuadCount);
    void publishPlanetPatchBatch(const PlanetPatchMeshBatch& batch);
    void startPlanetLodJob(wgen::PlanetLodPatchPlan plan);
    std::function<glm::vec3(float)> getActiveColorFunc() const;

    enum class TerrainJobKind {
        None,
        Regeneration,
        PlanetLodStreaming,
    };

    wgen::AppConfig config_{};
    wgen::GeneratorPipelineSpec pipelineSpec_{};
    wgen::Generator3dPipelineSpec planetPipelineSpec_{};
    ColorFunctions activeColorFuncId_{ColorFunctions::TerrainColorStandard};

    std::size_t usedGenerator_{0};
    std::vector<std::unique_ptr<wgen::Generator>> generators_{};
    std::unique_ptr<GpuTerrainPipeline> gpuPipeline_{};
    wgen::ThreadPool threadPool_{};
    wgen::HeightMap<float> activeHeightMap_{};
    wgen::TerrainFieldSnapshot activeTerrainField_{};

    std::future<TerrainJobResult> terrainGenerationJob_{};
    bool terrainJobRunning_{false};
    TerrainJobKind terrainJobKind_{TerrainJobKind::None};
    bool planetLodSelectionDirty_{true};
    std::uint64_t nextTerrainEpoch_{0};
    std::uint64_t activeTerrainEpoch_{0};
    std::uint64_t desiredPlanetRequestRevision_{0};
    wgen::PlanetLodCoordinator planetLodCoordinator_{};
    std::mutex generatorsMutex_{};
};

} // namespace lve
