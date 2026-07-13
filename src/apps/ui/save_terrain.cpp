#include "save_terrain.hpp"

#include "ui_save_terrain.h"
#include "files/file_path.hpp"


#include <QFileDialog>
#include <QFileInfo>


namespace lve {

SaveTerrainDialog::SaveTerrainDialog(QWidget* parent)
    : QDialog{parent}, ui_{std::make_unique<Ui::SaveTerrainDialog>()} {
    ui_->setupUi(this);
    // setWindowTitle(generatorName(spec_.kind) + QStringLiteral(" Settings"));

    connect(ui_->filePathBrowsePushButton, &QPushButton::clicked, this, &SaveTerrainDialog::selectSaveLocation);


    
    connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

}

SaveTerrainDialog::~SaveTerrainDialog() = default;

void SaveTerrainDialog::accept() {
    cfg_.dataFormat = dataFromatFromIndex(ui_->styleComboBox->currentIndex());
    cfg_.fileFormat = exportFormatFromIndex(ui_->formatComboBox->currentIndex());
    cfg_.path = wgen::files::Path(ui_->filePathLineEdit->text().toStdString());
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

void SaveTerrainDialog::selectSaveLocation() {
    auto format = getFileFormat(exportFormatFromIndex(ui_->formatComboBox->currentIndex()));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save file"),
        ui_->filePathLineEdit->text().isEmpty()
            ? QDir::homePath() + "/output" + QString::fromStdString(format.second)
            : ui_->filePathLineEdit->text(),
        tr((format.first + ";;All files (*)").c_str())
    );

    if (filePath.isEmpty()) {
        return; // user cancelled
    }

    ui_->filePathLineEdit->setText(filePath);
}

std::pair<std::string, std::string> SaveTerrainDialog::getFileFormat(ExportFormats fmt) {
    std::pair<std::string, std::string> result;
    switch (fmt) {
        case ExportFormats::CSV:
            result.first = "CSV files (*.csv)";
            result.second = ".csv";
            break;
        case ExportFormats::PNG:
            result.first = "PNG files (*.png)";
            result.second = ".png";
            break;
        default:
            result.first = "PNG files (*.png)";
            result.second = ".png";
            break;
    }
    return result;
}

}