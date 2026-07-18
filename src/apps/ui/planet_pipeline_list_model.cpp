#include "planet_pipeline_list_model.hpp"

#include <QString>
#include <utility>

namespace lve {

PlanetPipelineListModel::PlanetPipelineListModel(QObject* parent)
    : QAbstractListModel{parent} {}

PlanetPipelineListModel::PlanetPipelineListModel(wgen::Generator3dPipelineSpec pipeline, QObject* parent)
    : QAbstractListModel{parent}, pipeline_{std::move(pipeline)} {}

int PlanetPipelineListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(pipeline_.size());
}

QVariant PlanetPipelineListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }

    const int row = index.row();
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    return displayName(pipeline_[static_cast<std::size_t>(row)]);
}

const wgen::Generator3dSpec* PlanetPipelineListModel::generatorAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }

    return &pipeline_[static_cast<std::size_t>(row)];
}

void PlanetPipelineListModel::updateGenerator(int row, wgen::Generator3dSpec spec) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    pipeline_[static_cast<std::size_t>(row)] = std::move(spec);
    const QModelIndex changedIndex = index(row);
    emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole});
}

void PlanetPipelineListModel::appendGenerator(wgen::Generator3dSpec spec) {
    const int row = rowCount();
    beginInsertRows(QModelIndex{}, row, row);
    pipeline_.push_back(std::move(spec));
    endInsertRows();
}

void PlanetPipelineListModel::removeGenerator(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    beginRemoveRows(QModelIndex{}, row, row);
    pipeline_.erase(pipeline_.begin() + row);
    endRemoveRows();
}

void PlanetPipelineListModel::clear() {
    if (pipeline_.empty()) {
        return;
    }

    beginRemoveRows(QModelIndex{}, 0, rowCount() - 1);
    pipeline_.clear();
    endRemoveRows();
}

QString PlanetPipelineListModel::displayName(const wgen::Generator3dSpec& spec) {
    switch (spec.kind) {
        case wgen::Generator3dKind::PerlinNoise:
            return QStringLiteral("3D Perlin (terrain LOD %1)")
                .arg(static_cast<unsigned int>(
                    wgen::generator3dFirstFullyVisibleDetail(spec)));
    }

    return QStringLiteral("Unknown generator");
}

} // namespace lve
