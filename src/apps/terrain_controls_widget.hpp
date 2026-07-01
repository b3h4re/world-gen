#pragma once

#include <QWidget>

#include <functional>
#include <memory>

namespace Ui {
class TerrainControlsWidget;
}

namespace lve {

class TerrainPipelineListModel;

class TerrainControlsWidget : public QWidget {
    Q_OBJECT

public:
    struct Callbacks {
        std::function<void()> regenerateTerrain;
        std::function<void()> reloadTerrain;
        std::function<void()> switchColor;
    };

    explicit TerrainControlsWidget(Callbacks callbacks, QWidget* parent = nullptr);
    ~TerrainControlsWidget() override;

    TerrainControlsWidget(const TerrainControlsWidget&) = delete;
    TerrainControlsWidget& operator=(const TerrainControlsWidget&) = delete;

    QWidget& vulkanWidget();

private:
    std::unique_ptr<Ui::TerrainControlsWidget> ui_;
    std::unique_ptr<TerrainPipelineListModel> pipelineModel_;
    Callbacks callbacks_;
};

} // namespace lve
