#include "apt_source_repository_item.h"
#include "ui_apt_source_repository_item.h"
#include "utilities.h"
#include "Utils/command_util.h"
#include <QDebug>
#include <QRegularExpression>

APTSourceRepositoryItem::~APTSourceRepositoryItem()
{
    delete ui;
}

APTSourceRepositoryItem::APTSourceRepositoryItem(APTSourcePtr aptSource, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::APTSourceRepositoryItem),
    mAptSource(aptSource)
{
    init();
}

void APTSourceRepositoryItem::init()
{
    ui->setupUi(this);

    Utilities::addDropShadow(this, 30, 10);

#ifdef Q_OS_MAC
    // Homebrew packages can't be enabled/disabled — hide the toggle
    ui->checkAptSource->hide();
    {
        // Build display: "Name — description" with (Cask) suffix for cask packages
        QString display = mAptSource->source;
        if (!mAptSource->components.isEmpty())
            display += QString::fromUtf8(" \u2014 ") + mAptSource->components;
        if (mAptSource->isSource)
            display += tr(" (Cask)");
        ui->lblAptSourceName->setText(display);
    }
#else
    ui->checkAptSource->setChecked(mAptSource->isActive);

    // example "deb [arch=amd64] http://packages.microsoft.com/repos/vscode stable main"
    QString source = mAptSource->source;

    source.remove(QRegularExpression("\\s[\\[]+.*[\\]]+"));

    if (mAptSource->isSource) {
        ui->lblAptSourceName->setText(tr("%1 (Source Code)").arg(source));
    } else {
        ui->lblAptSourceName->setText(source);
    }
#endif

    ui->lblAptSourceName->setToolTip(ui->lblAptSourceName->text());
}

APTSourcePtr APTSourceRepositoryItem::aptSource() const
{
    return mAptSource;
}

void APTSourceRepositoryItem::on_checkAptSource_clicked(bool checked)
{
    ToolManager::ins()->changeAPTStatus(mAptSource, checked);
}
