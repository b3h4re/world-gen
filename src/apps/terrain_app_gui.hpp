#pragma once

#include <functional>

class QWidget;

namespace lve {

class TerrainAppGui {
public:
    struct Callbacks {
        std::function<void()> regenerateTerrain;
        std::function<void()> reloadTerrain;
        std::function<void()> switchColor;
    };

    TerrainAppGui(QWidget& parent, Callbacks callbacks);

    TerrainAppGui(const TerrainAppGui&) = delete;
    TerrainAppGui& operator=(const TerrainAppGui&) = delete;

private:
    QWidget& parent_;
};

} // namespace lve
