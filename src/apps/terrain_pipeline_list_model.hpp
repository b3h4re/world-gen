#pragma once

#include "terrain/generators/generator_spec.hpp"

#include <QAbstractListModel>

namespace lve {

class TerrainPipelineListModel : public QAbstractListModel {
public:
    explicit TerrainPipelineListModel(QObject* parent = nullptr);
    explicit TerrainPipelineListModel(wgen::GeneratorPipelineSpec pipeline, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setPipeline(wgen::GeneratorPipelineSpec pipeline);
    const wgen::GeneratorPipelineSpec& pipeline() const { return pipeline_; }

private:
    static QString displayName(const wgen::GeneratorSpec& spec);

    wgen::GeneratorPipelineSpec pipeline_;
};

} // namespace lve
