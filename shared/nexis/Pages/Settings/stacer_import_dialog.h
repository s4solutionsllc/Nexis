#pragma once

#include <QDialog>
#include "Utils/stacer_importer.h"

namespace Ui {
class StacerImportDialog;
}

class StacerImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StacerImportDialog(const StacerImportResult &result, QWidget *parent = nullptr);
    ~StacerImportDialog();

private:
    Ui::StacerImportDialog *ui;
    void populate(const StacerImportResult &result);
};
