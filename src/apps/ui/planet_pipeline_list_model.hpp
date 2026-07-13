#pragma once

#include "terrain/generators/3d/generator_spec.hpp"

#include <QAbstractListModel>

namespace lve {

class PlanetPipelineListModel : public QAbstractListModel {
public:
    explicit PlanetPipelineListModel(QObject* parent = nullptr);
    explicit PlanetPipelineListModel(wgen::Generator3dPipelineSpec pipeline, QObject* parent = nullptr);

    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    const wgen::Generator3dSpec* generatorAt(int row) const;
    void updateGenerator(int row, wgen::Generator3dSpec spec);
    void appendGenerator(wgen::Generator3dSpec spec);
    void removeGenerator(int row);
    const wgen::Generator3dPipelineSpec& pipeline() const { return pipeline_; }

private:
    static QString displayName(const wgen::Generator3dSpec& spec);

    wgen::Generator3dPipelineSpec pipeline_;
};

} // namespace lve
