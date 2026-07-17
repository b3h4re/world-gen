#pragma once

#include "apps/terrain_app_core.hpp"
#include "config/app_config.hpp"
#include "device/lve_device.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/objects/game_object_3d.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "renderer/lve_renderer.hpp"
#include "renderer/objects/local_clipmap_render_resources.hpp"
#include "window/qt_window_backend.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace lve {

struct RetiredFrameObjects {
    std::vector<GameObject2d> objects2d;
    std::vector<GameObject3d> objects3d;
    std::vector<GameObject3d> objectsPlanet;
    std::vector<GameObject3d> objectsLocalClipmap;

    void clear() {
        objects2d.clear();
        objects3d.clear();
        objectsPlanet.clear();
        objectsLocalClipmap.clear();
    }
};

struct ResidentPlanetPatch {
    GameObject3d object{};
    PlanetPatchVersion version{};
};

class TerrainAppRenderer {
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    TerrainAppRenderer();
    explicit TerrainAppRenderer(
        const wgen::WindowConfig& config,
        wgen::LocalClipmapConfig localClipmapConfig = {});
    ~TerrainAppRenderer();

    TerrainAppRenderer(const TerrainAppRenderer&) = delete;
    TerrainAppRenderer& operator=(const TerrainAppRenderer&) = delete;

    void applyTerrainMesh(int frameIndex, TerrainPlaneMeshData data);
    void applyPlanetPatchBatch(int frameIndex, PlanetPatchMeshBatch batch);
    void applyLocalClipmapHeightUpload(
        const wgen::LocalClipmapGpuUploadBatch& batch);
    void applyLocalClipmapMeshUpdate(
        int frameIndex,
        LocalClipmapMeshUpdate update);
    void setPlanetDrawPatches(std::span<const wgen::PlanetPatchDrawState> states);
    void clearRetiredObjects(int frameIndex);
    void waitIdle();
    void shutdownVulkanResources();
    void setDesiredPresentMode(PresentMode desiredPresentMode);

    QtWindowBackend& window() { return window_; }
    WindowSurface& windowSurface() { return window_; }
    LveDevice& device() { return device_; }
    LveRenderer& renderer() { return renderer_; }
    LveDescriptorPool& descriptorPool() { return *globalPool_; }
    ColorMapper& colorMapper() { return colorMapper_; }

    const std::vector<GameObject2d>& objects2d() const { return objects2d_; }
    const std::vector<GameObject3d>& objects3d() const { return objects3d_; }
    const std::vector<GameObject3d>& objectsPlanet() const { return objectsPlanet_; }
    const std::vector<LocalClipmapRenderObject>& objectsLocalClipmap() const {
        return localClipmapResources_.objects();
    }
    std::size_t residentPlanetPatchCount() const { return residentPlanetPatches_.size(); }

private:
    void initDescriptorPool();
    std::vector<GameObject2d> makeObjects2d(const TerrainPlaneMeshData& data);
    std::vector<GameObject3d> makeObjects3d(const TerrainPlaneMeshData& data);

    QtWindowBackend window_;
    LveDevice device_;
    LveRenderer renderer_;
    ColorMapper colorMapper_;
    LocalClipmapRenderResources localClipmapResources_;
    PresentMode desiredPresentMode_{PresentMode::VSync};
    std::unique_ptr<LveDescriptorPool> globalPool_{};
    std::vector<GameObject2d> objects2d_{};
    std::vector<GameObject3d> objects3d_{};
    std::unordered_map<
        wgen::PlanetPatchId,
        ResidentPlanetPatch,
        wgen::PlanetPatchIdHash> residentPlanetPatches_{};
    std::vector<wgen::PlanetPatchDrawState> planetDrawOrder_{};
    std::vector<GameObject3d> objectsPlanet_{};
    std::shared_ptr<const Mesh3dIndexSet> planetPatchIndexSet_{};
    std::uint32_t planetPatchIndexQuadCount_{};
    std::uint64_t activePlanetEpoch_{0};
    std::array<RetiredFrameObjects, LveSwapChain::MAX_FRAMES_IN_FLIGHT> retiredObjects_{};
    bool vulkanResourcesShutdown_{false};
};

} // namespace lve
