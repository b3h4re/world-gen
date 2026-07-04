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

const wgen::GeneratorSpec* TerrainPipelineListModel::generatorAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }

    return &pipeline_[static_cast<std::size_t>(row)];
}

void TerrainPipelineListModel::updateGenerator(int row, wgen::GeneratorSpec spec) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    pipeline_[static_cast<std::size_t>(row)] = std::move(spec);
    const QModelIndex changedIndex = index(row);
    emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole});
}

void TerrainPipelineListModel::appendGenerator(wgen::GeneratorSpec spec) {
    const int row = rowCount();
    beginInsertRows(QModelIndex{}, row, row);
    pipeline_.push_back(std::move(spec));
    endInsertRows();
}

void TerrainPipelineListModel::removeGenerator(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    beginRemoveRows(QModelIndex{}, row, row);
    pipeline_.erase(pipeline_.begin() + row);
    endRemoveRows();
}

QString TerrainPipelineListModel::displayName(const wgen::GeneratorSpec& spec) {
    switch (spec.kind) {
        case wgen::GeneratorKind::PerlinNoise:
            return QStringLiteral("Perlin");
        case wgen::GeneratorKind::GaborNoise:
            return QStringLiteral("Gabor");
        case wgen::GeneratorKind::SimplexNoise:
            return QStringLiteral("Simplex");
        case wgen::GeneratorKind::WorleyNoise:
            return QStringLiteral("Worley");
        case wgen::GeneratorKind::ValueNoise:
            return QStringLiteral("Value noise");
        case wgen::GeneratorKind::WaveletNoise:
            return QStringLiteral("Wavelet noise");
    }

    return QStringLiteral("Unknown generator");
}

} // namespace lve
