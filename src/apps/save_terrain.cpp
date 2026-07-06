#include "save_terrain.hpp"

#include "ui_save_terrain.h"



namespace lve {

SaveTerrainDialog::SaveTerrainDialog(QWidget* parent)
    : QDialog{parent}, ui_{std::make_unique<Ui::SaveTerrainDialog>()} {
    ui_->setupUi(this);
    // setWindowTitle(generatorName(spec_.kind) + QStringLiteral(" Settings"));



    
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

}

SaveTerrainDialog::~SaveTerrainDialog() = default;

void SaveTerrainDialog::accept() {
    cfg_.dataFormat = dataFromatFromIndex(ui_->styleComboBox->currentIndex());
    cfg_.fileFormat = exportFormatFromIndex(ui_->formatComboBox->currentIndex());
    QDialog::accept();
}

DataFormats SaveTerrainDialog::dataFromatFromIndex(int index) {
    switch (index) {
        case 0:
            return DataFormats::TwoDim;
        case 1:
            return DataFormats::ThreeDim;
        default:
            return DataFormats::TwoDim;
    }
}

ExportFormats SaveTerrainDialog::exportFormatFromIndex(int index) {
    switch (index) {
        case 0:
            return ExportFormats::PNG;
        case 1:
            return ExportFormats::CSV;
        default:
            return ExportFormats::PNG;
    }
}

}