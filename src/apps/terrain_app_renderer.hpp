#pragma once

#include "apps/terrain_app_core.hpp"
#include "config/app_config.hpp"
#include "device/lve_device.hpp"
#include "game/objects/game_object_2d.hpp"
#include "game/objects/game_object_3d.hpp"
#include "pipeline/descriptors/lve_descriptors.hpp"
#include "renderer/lve_renderer.hpp"
#include "window/qt_window_backend.hpp"

#include <array>
#include <memory>
#include <vector>

namespace lve {

struct RetiredFrameObjects {
    std::vector<GameObject2d> objects2d;
    std::vector<GameObject3d> objects3d;

    void clear() {
        objects2d.clear();
        objects3d.clear();
    }
};

class TerrainAppRenderer {
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    TerrainAppRenderer();
    explicit TerrainAppRenderer(const wgen::WindowConfig& config);
    ~TerrainAppRenderer();

    TerrainAppRenderer(const TerrainAppRenderer&) = delete;
    TerrainAppRenderer& operator=(const TerrainAppRenderer&) = delete;

    void setTerrainMesh(TerrainMeshData data);
    void applyTerrainMesh(int frameIndex, TerrainMeshData data);
    void clearRetiredObjects(int frameIndex);
    void waitIdle();
    void shutdownVulkanResources();
    void setDesiredPresentMode(PresentMode desiredPresentMode);

    QtWindowBackend& window() { return window_; }
    WindowSurface& windowSurface() { return window_; }
    LveDevice& device() { return device_; }
    LveRenderer& renderer() { return renderer_; }
    LveDescriptorPool& descriptorPool() { return *globalPool_; }

    const std::vector<GameObject2d>& objects2d() const { return objects2d_; }
    const std::vector<GameObject3d>& objects3d() const { return objects3d_; }

private:
    void initDescriptorPool();
    std::vector<GameObject2d> makeObjects2d(const TerrainMeshData& data);
    std::vector<GameObject3d> makeObjects3d(const TerrainMeshData& data);

    QtWindowBackend window_;
    LveDevice device_;
    LveRenderer renderer_;
    PresentMode desiredPresentMode_{PresentMode::VSync};
    std::unique_ptr<LveDescriptorPool> globalPool_{};
    std::vector<GameObject2d> objects2d_{};
    std::vector<GameObject3d> objects3d_{};
    std::array<RetiredFrameObjects, LveSwapChain::MAX_FRAMES_IN_FLIGHT> retiredObjects_{};
    bool vulkanResourcesShutdown_{false};
};

} // namespace lve
