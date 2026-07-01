#include "terrain_pipeline_list_model.hpp"

#include <QString>
#include <utility>

namespace lve {

TerrainPipelineListModel::TerrainPipelineListModel(QObject* parent)
    : QAbstractListModel{parent} {}

TerrainPipelineListModel::TerrainPipelineListModel(wgen::GeneratorPipelineSpec pipeline, QObject* parent)
    : QAbstractListModel{parent}, pipeline_{std::move(pipeline)} {}

int TerrainPipelineListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(pipeline_.size());
}

QVariant TerrainPipelineListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    return displayName(pipeline_[static_cast<std::size_t>(row)]);
}

void TerrainPipelineListModel::setPipeline(wgen::GeneratorPipelineSpec pipeline) {
    beginResetModel();
    pipeline_ = std::move(pipeline);
    endResetModel();
}

QString TerrainPipelineListModel::displayName(const wgen::GeneratorSpec& spec) {
    switch (spec.kind) {
        case wgen::GeneratorKind::PerlinNoise:
            return QStringLiteral("Perlin");
        case wgen::GeneratorKind::ValueNoise:
            return QStringLiteral("Value noise");
    }

    return QStringLiteral("Unknown generator");
}

} // namespace lve
