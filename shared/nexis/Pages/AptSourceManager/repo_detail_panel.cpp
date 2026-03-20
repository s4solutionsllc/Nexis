#include "repo_detail_panel.h"
#include "Managers/app_manager.h"
#include "signal_mapper.h"
#include "Utils/command_util.h"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollArea>
#include <QSettings>
#include <QDateTime>

RepoDetailPanel::RepoDetailPanel(QWidget *parent)
    : QWidget(parent),
      mSignalMapper(SignalMapper::ins())
{
    setupUi();
    connect(mSignalMapper, &SignalMapper::sigChangedAppTheme,
            this, &RepoDetailPanel::refreshThemeColors);
}

void RepoDetailPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // Header row: name label, status badge pill, close button
    QHBoxLayout *headerRow = new QHBoxLayout();
    mLblName = new QLabel(this);
    mLblName->setObjectName("repoDetailName");
    QFont nameFont = mLblName->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    mLblName->setFont(nameFont);
    mLblName->setWordWrap(true);
    headerRow->addWidget(mLblName, 1);

    mLblStatusBadge = new QLabel(this);
    mLblStatusBadge->setObjectName("repoStatusBadge");
    mLblStatusBadge->setFixedHeight(20);
    mLblStatusBadge->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(mLblStatusBadge);

    mBtnClose = new QPushButton(QString::fromUtf8("\u2715"), this);
    mBtnClose->setFixedSize(24, 24);
    mBtnClose->setFocusPolicy(Qt::NoFocus);
    mBtnClose->setCursor(Qt::PointingHandCursor);
    mBtnClose->setFlat(true);
    connect(mBtnClose, &QPushButton::clicked, this, &RepoDetailPanel::closeRequested);
    headerRow->addWidget(mBtnClose);

    mainLayout->addLayout(headerRow);

    // Description
    mLblDescription = new QLabel(this);
    mLblDescription->setObjectName("repoDetailDescription");
    mLblDescription->setWordWrap(true);
    mainLayout->addWidget(mLblDescription);

    // Metadata grid
    mMetadataWidget = new QWidget(this);
    QGridLayout *metaGrid = new QGridLayout(mMetadataWidget);
    metaGrid->setContentsMargins(0, 0, 0, 0);
    metaGrid->setSpacing(8);

    auto addMetaField = [&](int row, int col, const QString &label, QLabel *&valueLabel) {
        QLabel *lbl = new QLabel(label, mMetadataWidget);
        lbl->setStyleSheet("font-size: 9px; text-transform: uppercase;");
        metaGrid->addWidget(lbl, row * 2, col);
        valueLabel = new QLabel(mMetadataWidget);
        valueLabel->setObjectName("metaValue");
        metaGrid->addWidget(valueLabel, row * 2 + 1, col);
    };

    addMetaField(0, 0, tr("STATUS"), mLblStatus);
    addMetaField(0, 1, tr("LAST CHECKED"), mLblLastChecked);
#ifdef Q_OS_LINUX
    addMetaField(0, 2, tr("FILE"), mLblFile);
    addMetaField(1, 0, tr("SUITE"), mLblSuite);
    addMetaField(1, 1, tr("FORMAT"), mLblFormat);
#endif

    mainLayout->addWidget(mMetadataWidget);

    // Issues section (scrollable) — apply CLAUDE.md QScrollArea viewport fix
    QScrollArea *issueScroll = new QScrollArea(this);
    issueScroll->setFrameShape(QFrame::NoFrame);
    issueScroll->setWidgetResizable(true);
    issueScroll->setStyleSheet("QScrollArea{background-color:transparent;}");

    mIssuesContainer = new QWidget();
    mIssuesContainer->setStyleSheet("background-color:transparent;");
    mIssuesLayout = new QVBoxLayout(mIssuesContainer);
    mIssuesLayout->setContentsMargins(0, 0, 0, 0);
    mIssuesLayout->setSpacing(6);
    mIssuesLayout->addStretch();

    issueScroll->setWidget(mIssuesContainer);
    mainLayout->addWidget(issueScroll, 1);

    // Action buttons
    QHBoxLayout *actionRow = new QHBoxLayout();
#ifdef Q_OS_LINUX
    mBtnEdit = new QPushButton(tr("Edit"), this);
    mBtnEdit->setAccessibleName("primary");
    mBtnEdit->setCursor(Qt::PointingHandCursor);
    mBtnEdit->setFocusPolicy(Qt::NoFocus);
    connect(mBtnEdit, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit editRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnEdit);

    mBtnOpenUri = new QPushButton(tr("Open URI"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    mBtnOpenUri->setFocusPolicy(Qt::NoFocus);
    connect(mBtnOpenUri, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            QDesktopServices::openUrl(QUrl(mCurrentSource->uri));
    });
    actionRow->addWidget(mBtnOpenUri);

    mBtnDisable = new QPushButton(tr("Disable"), this);
    mBtnDisable->setAccessibleName("danger");
    mBtnDisable->setCursor(Qt::PointingHandCursor);
    mBtnDisable->setFocusPolicy(Qt::NoFocus);
    connect(mBtnDisable, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit disableRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnDisable);
#else
    // macOS actions
    mBtnOpenUri = new QPushButton(tr("Open Homepage"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    mBtnOpenUri->setFocusPolicy(Qt::NoFocus);
    connect(mBtnOpenUri, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            QDesktopServices::openUrl(QUrl(mCurrentSource->uri));
    });
    actionRow->addWidget(mBtnOpenUri);
#endif

    actionRow->addStretch();
    mainLayout->addLayout(actionRow);
}

void RepoDetailPanel::showRepo(const APTSourcePtr &source, const RepoHealthResult &result)
{
    mCurrentSource = source;
    mCurrentResult = result;

    mLblName->setText(result.name.isEmpty() ? source->uri : result.name);
    mLblDescription->setText(result.description);

    // Status badge — use QSettings* API
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString statusText, statusColor;
    switch (result.status) {
    case RepoHealthResult::Healthy:
        statusText = tr("Healthy");
        statusColor = sv ? sv->value("@successColor").toString() : QString();
        break;
    case RepoHealthResult::Warning:
        statusText = tr("Warning");
        statusColor = sv ? sv->value("@warningColor").toString() : QString();
        break;
    case RepoHealthResult::Error:
        statusText = tr("Error");
        statusColor = sv ? sv->value("@destructiveColor").toString() : QString();
        break;
    default:
        statusText = tr("Unknown");
        statusColor = sv ? sv->value("@tertiaryText").toString() : QString();
        break;
    }
    mLblStatusBadge->setText(statusText);
    mLblStatusBadge->setStyleSheet(QString(
        "background-color: %1; color: white; border-radius: 10px; padding: 2px 10px; font-size: 11px;"
    ).arg(statusColor));

    // Metadata
    mLblStatus->setText(statusText);
    mLblStatus->setStyleSheet(QString("color: %1;").arg(statusColor));

    if (result.lastChecked.isValid()) {
        qint64 secsAgo = result.lastChecked.secsTo(QDateTime::currentDateTime());
        if (secsAgo < 60)
            mLblLastChecked->setText(tr("Just now"));
        else if (secsAgo < 3600)
            mLblLastChecked->setText(tr("%1 min ago").arg(secsAgo / 60));
        else
            mLblLastChecked->setText(tr("%1 hr ago").arg(secsAgo / 3600));
    } else {
        mLblLastChecked->setText(tr("Never"));
    }

#ifdef Q_OS_LINUX
    if (mLblFile) {
        QFileInfo fi(source->filePath);
        mLblFile->setText(fi.fileName());
        mLblFile->setToolTip(source->filePath);
    }
    if (mLblSuite)
        mLblSuite->setText(source->suites);
    if (mLblFormat)
        mLblFormat->setText(source->format == APTSource::Deb822 ? "deb822" : "legacy .list");

    if (mBtnDisable)
        mBtnDisable->setText(source->isActive ? tr("Disable") : tr("Enable"));
#endif

    // Issues
    clearIssues();
    for (const RepoHealthIssue &issue : result.issues)
        addIssueWidget(issue);

    show();
}

void RepoDetailPanel::addIssueWidget(const RepoHealthIssue &issue)
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    QString color;
    switch (issue.severity) {
    case RepoHealthIssue::Error:
        color = sv ? sv->value("@destructiveColor").toString() : QString();
        break;
    case RepoHealthIssue::Warning:
        color = sv ? sv->value("@warningColor").toString() : QString();
        break;
    default:
        color = sv ? sv->value("@tertiaryText").toString() : QString();
        break;
    }

    QWidget *issueWidget = new QWidget(mIssuesContainer);
    issueWidget->setStyleSheet(QString(
        "background-color: rgba(0,0,0,0.1); border-radius: 4px; border-left: 3px solid %1;"
    ).arg(color));

    QVBoxLayout *issueLayout = new QVBoxLayout(issueWidget);
    issueLayout->setContentsMargins(10, 8, 10, 8);
    issueLayout->setSpacing(4);

    QLabel *lblSummary = new QLabel(issue.summary, issueWidget);
    lblSummary->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
    issueLayout->addWidget(lblSummary);

    if (!issue.detail.isEmpty()) {
        QLabel *lblDetail = new QLabel(issue.detail, issueWidget);
        lblDetail->setWordWrap(true);
        QString detailColor = sv ? sv->value("@tertiaryText").toString() : QString();
        lblDetail->setStyleSheet(QString("color: %1;").arg(detailColor));
        issueLayout->addWidget(lblDetail);
    }

    if (!issue.repairCmd.isEmpty()) {
        QPushButton *btnRepair = new QPushButton(
            issue.repairLabel.isEmpty() ? tr("Repair") : issue.repairLabel, issueWidget);
        btnRepair->setAccessibleName("primary");
        btnRepair->setCursor(Qt::PointingHandCursor);
        btnRepair->setFocusPolicy(Qt::NoFocus);
        btnRepair->setFixedHeight(26);
        QString cmd = issue.repairCmd;
        QString label = issue.repairLabel;
        connect(btnRepair, &QPushButton::clicked, this, [this, cmd, label]() {
            emit repairRequested(cmd, label);
        });
        issueLayout->addWidget(btnRepair, 0, Qt::AlignLeft);
    }

    // Insert before the stretch
    mIssuesLayout->insertWidget(mIssuesLayout->count() - 1, issueWidget);
}

void RepoDetailPanel::clearIssues()
{
    // Remove all widgets except the stretch at the end
    while (mIssuesLayout->count() > 1) {
        QLayoutItem *item = mIssuesLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void RepoDetailPanel::clear()
{
    mCurrentSource.clear();
    mLblName->clear();
    mLblDescription->clear();
    mLblStatusBadge->clear();
    mLblStatus->clear();
    mLblLastChecked->clear();
    clearIssues();
    hide();
}

void RepoDetailPanel::refreshThemeColors()
{
    if (mCurrentSource) {
        // Full re-render: showRepo handles all color/style application
        showRepo(mCurrentSource, mCurrentResult);
    }
}
