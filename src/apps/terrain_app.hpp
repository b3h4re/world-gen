#pragma once

#include "apps/terrain_app_core.hpp"
#include "apps/terrain_app_gui.hpp"
#include "apps/terrain_app_renderer.hpp"
#include "config/app_config.hpp"

namespace lve {

class TerrainApp {
public:
    TerrainApp();
    explicit TerrainApp(const wgen::AppConfig& config);
    ~TerrainApp();

    TerrainApp(const TerrainApp&) = delete;
    TerrainApp& operator=(const TerrainApp&) = delete;

    void run();

private:
    void applyFinishedTerrainJob(int frameIndex);
    void regenerateWithRandomSeed();
    void reloadConfiguredSeed();

    TerrainAppCore core_;
    TerrainAppRenderer renderer_;
    TerrainAppGui gui_;
    bool render3d_{false};
};

} // namespace lve
