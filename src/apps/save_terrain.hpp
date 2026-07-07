#pragma once

#include "files/exporter.hpp"

#include <QDialog>
#include <QSpinBox>

#include <memory>


namespace Ui {
class SaveTerrainDialog;
}

namespace lve {

class SaveTerrainDialog : public QDialog {
public:
    explicit SaveTerrainDialog(QWidget* parent = nullptr);
    ~SaveTerrainDialog() override;


    ExportConfig getConfig() const { return cfg_; }

protected:
    void accept() override;

private:
    std::unique_ptr<Ui::SaveTerrainDialog> ui_;
    ExportConfig cfg_;

    DataFormats dataFromatFromIndex(int index);
    ExportFormats exportFormatFromIndex(int index);

    QWidget* getLabelForField(QWidget *field);

    std::pair<std::string, std::string> getFileFormat(ExportFormats fmt);

    void selectSaveLocation();

};

} // namespace lve
