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
    QDialog::accept();
}

}