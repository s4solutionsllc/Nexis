#include "apt_source_repository_item.h"
#include "ui_apt_source_repository_item.h"
#include "utilities.h"
#include "Utils/command_util.h"
#include "signal_mapper.h"
#include <QDebug>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QStyle>
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

    // Create status indicator (shape icon + color for accessibility)
    mStatusDot = new QLabel(this);
    mStatusDot->setObjectName("repoStatusDot");
    mStatusDot->setFixedSize(14, 14);
    mStatusDot->setAlignment(Qt::AlignCenter);
    mStatusDot->setAccessibleName("statusDot");
    updateStatusIndicator(RepoHealthResult::Unknown);

    // Create description label
    mLblDescription = new QLabel(this);
    mLblDescription->setObjectName("lblRepoDescription");
    mLblDescription->setWordWrap(false);
    mLblDescription->setTextFormat(Qt::PlainText);
    mLblDescription->setTextInteractionFlags(Qt::NoTextInteraction);
    mLblDescription->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mLblDescription->setMinimumWidth(0);
    QFont descFont = mLblDescription->font();
    descFont.setPointSize(descFont.pointSize() - 1);
    mLblDescription->setFont(descFont);

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
    // Build full description string
    QString desc;
    if (!result.description.isEmpty()) {
        desc = result.name.isEmpty() ? result.description
             : result.name + QString::fromUtf8(" \u2014 ") + result.description;
    }

    // Append inline issue summary for warnings/errors
    if (!result.issues.isEmpty() && result.status != RepoHealthResult::Healthy) {
        if (!desc.isEmpty())
            desc += QString::fromUtf8(" \u2014 ");
        desc += result.issues.first().summary;
    }

    // Store full text and apply elision
    mFullDescription = desc;
    mLblDescription->setToolTip(desc);
    elideDescription();

    // Update status indicator
    updateStatusIndicator(result.status);
    mCurrentStatus = result.status;

    // Accessibility
    QString statusText;
    switch (result.status) {
    case RepoHealthResult::Healthy: statusText = tr("Healthy"); break;
    case RepoHealthResult::Warning: statusText = tr("Warning"); break;
    case RepoHealthResult::Error:   statusText = tr("Error"); break;
    default:                         statusText = tr("Unknown"); break;
    }
    setAccessibleDescription(statusText + ": " + mFullDescription);
}

void APTSourceRepositoryItem::updateStatusIndicator(RepoHealthResult::Status status)
{
    QString statusStr;
    QString icon;
    switch (status) {
    case RepoHealthResult::Healthy:
        statusStr = "success";
        icon  = QString::fromUtf8("\u2713"); // checkmark
        break;
    case RepoHealthResult::Warning:
        statusStr = "warning";
        icon  = QString::fromUtf8("\u25B2"); // triangle
        break;
    case RepoHealthResult::Error:
        statusStr = "error";
        icon  = QString::fromUtf8("\u2717"); // X mark
        break;
    default:
        statusStr = "neutral";
        icon  = "?";
        break;
    }

    mStatusDot->setText(icon);
    mStatusDot->setProperty("status", statusStr);
    mStatusDot->style()->unpolish(mStatusDot);
    mStatusDot->style()->polish(mStatusDot);

    ui->aptSourceRepositoryItemWidget->setProperty("repoStatus", statusStr);
    ui->aptSourceRepositoryItemWidget->style()->unpolish(ui->aptSourceRepositoryItemWidget);
    ui->aptSourceRepositoryItemWidget->style()->polish(ui->aptSourceRepositoryItemWidget);
}

void APTSourceRepositoryItem::refreshThemeColors()
{
    updateStatusIndicator(mCurrentStatus);
}

void APTSourceRepositoryItem::elideDescription()
{
    if (mFullDescription.isEmpty()) {
        mLblDescription->clear();
        return;
    }
    int availWidth = mLblDescription->width() > 0 ? mLblDescription->width() : 400;
    QFontMetrics fm(mLblDescription->font());
    mLblDescription->setText(fm.elidedText(mFullDescription, Qt::ElideRight, availWidth));
}

void APTSourceRepositoryItem::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    elideDescription();
}

void APTSourceRepositoryItem::on_checkAptSource_clicked(bool checked)
{
    ToolManager::ins()->changeAPTStatus(mAptSource, checked);
}
