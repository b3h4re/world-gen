#pragma once

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

protected:
    void accept() override;

private:
    std::unique_ptr<Ui::SaveTerrainDialog> ui_;

    QWidget* getLabelForField(QWidget *field);

};

} // namespace lve
