#pragma once

#include "apps/terrain_app_core.hpp"
#include "apps/terrain_app_gui.hpp"
#include "apps/terrain_app_renderer.hpp"
#include "config/app_config.hpp"
#include "utils/frame_limiter.hpp"
#include "files/exporter.hpp"
#include "game/input/input_system.hpp"
#include "renderer/lve_frame_info.hpp"

namespace lve {

class TerrainApp {
public:
    TerrainApp();
    explicit TerrainApp(const wgen::AppConfig& config);
    ~TerrainApp();

    TerrainApp(const TerrainApp&) = delete;
    TerrainApp& operator=(const TerrainApp&) = delete;

    void run();

    wgen::AppConfig getConfig() const;

private:
    void applyWindowConfig(const wgen::WindowConfig& cfg);
    void applyPlanetShape(std::size_t resolution, float radius);
    void applyFinishedTerrainJob(int frameIndex);
    TerrainGenerationTarget currentGenerationTarget() const;
    void regenerateWithRandomSeed();
    void reloadConfiguredSeed();

    void rotateCameraViews();
    void updateCamerasStatus(std::vector<std::pair<CameraUpdateTarget, TerrainRenderModes>>& cameraTargets);

    wgen::AppConfig config_;
    TerrainAppCore core_;
    TerrainAppRenderer renderer_;
    TerrainAppGui gui_;
    FrameLimiter limiter_;
    Exporter exporter_;
    TerrainRenderModes renderMode_{TerrainRenderModes::FlatPicture};
};

} // namespace lve
