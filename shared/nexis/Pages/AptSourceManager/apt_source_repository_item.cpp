#include "apt_source_repository_item.h"
#include "ui_apt_source_repository_item.h"
#include "utilities.h"
#include "Utils/command_util.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include <QDebug>
#include <QRegularExpression>
#include <QVBoxLayout>

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

    // --- Enriched card: status dot + description line ---
    // Restructure layout: wrap name label in a VBox with description underneath
    QHBoxLayout *hLayout = ui->startupAppLayout;

    // Create status dot (8px colored circle)
    mStatusDot = new QLabel(this);
    mStatusDot->setFixedSize(8, 8);
    mStatusDot->setAccessibleName("statusDot");
    updateStatusIndicator(RepoHealthResult::Unknown);

    // Create description label
    mLblDescription = new QLabel(this);
    mLblDescription->setObjectName("lblRepoDescription");
    QFont descFont = mLblDescription->font();
    descFont.setPointSize(descFont.pointSize() - 1);
    mLblDescription->setFont(descFont);
    {
        QSettings *sv = AppManager::ins()->getStyleValues();
        mLblDescription->setStyleSheet("color: " + (sv ? sv->value("@tertiaryText").toString() : QString()) + ";");
    }

    // Create vertical layout for name + description
    QVBoxLayout *textVBox = new QVBoxLayout();
    textVBox->setSpacing(2);
    textVBox->setContentsMargins(0, 0, 0, 0);

    // Remove lblAptSourceName from the HBox, add to VBox
    hLayout->removeWidget(ui->lblAptSourceName);
    textVBox->addWidget(ui->lblAptSourceName);
    textVBox->addWidget(mLblDescription);

    // Insert status dot and text VBox into the HBox after the icon
    // Icon is at index 0, so insert dot at 1, text at 2
    hLayout->insertWidget(1, mStatusDot, 0, Qt::AlignVCenter);
    hLayout->insertLayout(2, textVBox, 1);

    // Connect theme changes
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &APTSourceRepositoryItem::refreshThemeColors);

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

void APTSourceRepositoryItem::setHealthResult(const RepoHealthResult &result)
{
    // Update description
    if (!result.description.isEmpty()) {
        QString desc = result.name.isEmpty() ? result.description
            : result.name + QString::fromUtf8(" \u2014 ") + result.description;
        mLblDescription->setText(desc);
        mLblDescription->setToolTip(desc);
    }

    // Update status indicator
    updateStatusIndicator(result.status);
    mCurrentStatus = result.status;

    // Show inline issue summary for warnings/errors
    if (!result.issues.isEmpty() && result.status != RepoHealthResult::Healthy) {
        QString issueSummary = result.issues.first().summary;
        mLblDescription->setText(mLblDescription->text() + " — " + issueSummary);
    }

    // Accessibility
    QString statusText;
    switch (result.status) {
    case RepoHealthResult::Healthy: statusText = tr("Healthy"); break;
    case RepoHealthResult::Warning: statusText = tr("Warning"); break;
    case RepoHealthResult::Error:   statusText = tr("Error"); break;
    default:                         statusText = tr("Unknown"); break;
    }
    setAccessibleDescription(statusText + ": " + mLblDescription->text());
}

void APTSourceRepositoryItem::updateStatusIndicator(RepoHealthResult::Status status)
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString color;
    switch (status) {
    case RepoHealthResult::Healthy: color = sv ? sv->value("@successColor").toString() : QString(); break;
    case RepoHealthResult::Warning: color = sv ? sv->value("@warningColor").toString() : QString(); break;
    case RepoHealthResult::Error:   color = sv ? sv->value("@destructiveColor").toString() : QString(); break;
    default:                         color = sv ? sv->value("@tertiaryText").toString() : QString(); break;
    }

    mStatusDot->setStyleSheet(QString("background-color: %1; border-radius: 4px;").arg(color));
    ui->aptSourceRepositoryItemWidget->setStyleSheet(
        QString("border-left: 3px solid %1;").arg(color));
}

void APTSourceRepositoryItem::refreshThemeColors()
{
    updateStatusIndicator(mCurrentStatus);
    QSettings *sv = AppManager::ins()->getStyleValues();
    mLblDescription->setStyleSheet("color: " + (sv ? sv->value("@tertiaryText").toString() : QString()) + ";");
}

void APTSourceRepositoryItem::on_checkAptSource_clicked(bool checked)
{
    ToolManager::ins()->changeAPTStatus(mAptSource, checked);
}
