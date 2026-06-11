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
#include <QStyle>

RepoDetailPanel::RepoDetailPanel(QWidget *parent)
    : QWidget(parent),
      mSignalMapper(SignalMapper::ins())
{
    setObjectName("repoDetailPanel");
    setAttribute(Qt::WA_StyledBackground, true);
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
    mMetadataWidget->setObjectName("repoDetailMetadata");
    QGridLayout *metaGrid = new QGridLayout(mMetadataWidget);
    metaGrid->setContentsMargins(0, 0, 0, 0);
    metaGrid->setSpacing(8);

    auto addMetaField = [&](int row, int col, const QString &label, QLabel *&valueLabel) {
        QLabel *lbl = new QLabel(label.toUpper(), mMetadataWidget);
        lbl->setObjectName("repoMetaLabel");
        metaGrid->addWidget(lbl, row * 2, col);
        valueLabel = new QLabel(mMetadataWidget);
        valueLabel->setObjectName("repoMetaValue");
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
    connect(mBtnEdit, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit editRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnEdit);

    mBtnOpenUri = new QPushButton(tr("Open URI"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    connect(mBtnOpenUri, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            QDesktopServices::openUrl(QUrl(mCurrentSource->uri));
    });
    actionRow->addWidget(mBtnOpenUri);

    mBtnDisable = new QPushButton(tr("Disable"), this);
    mBtnDisable->setAccessibleName("danger");
    mBtnDisable->setCursor(Qt::PointingHandCursor);
    connect(mBtnDisable, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            emit disableRequested(mCurrentSource);
    });
    actionRow->addWidget(mBtnDisable);
#else
    // macOS actions
    mBtnOpenUri = new QPushButton(tr("Open Homepage"), this);
    mBtnOpenUri->setCursor(Qt::PointingHandCursor);
    connect(mBtnOpenUri, &QPushButton::clicked, this, [this]() {
        if (mCurrentSource)
            QDesktopServices::openUrl(QUrl(mCurrentSource->uri));
    });
    actionRow->addWidget(mBtnOpenUri);
#endif

    actionRow->addStretch();
    mainLayout->addLayout(actionRow);
}

void RepoDetailPanel::showRepo(const APTSourcePtr &source, const RepoHealthResult &result,
                               const DiagnoseResult *diagnoseResult)
{
    mCurrentSource = source;
    mCurrentResult = result;

    mLblName->setText(result.name.isEmpty() ? source->uri : result.name);
    mLblDescription->setText(result.description);

    // Status badge — property-driven QSS (FR-89)
    QString statusText, statusProp;
    switch (result.status) {
    case RepoHealthResult::Healthy:
        statusText = tr("Healthy"); statusProp = "healthy"; break;
    case RepoHealthResult::Warning:
        statusText = tr("Warning"); statusProp = "warning"; break;
    case RepoHealthResult::Error:
        statusText = tr("Error"); statusProp = "error"; break;
    default:
        statusText = tr("Unknown"); statusProp = "unknown"; break;
    }
    mLblStatusBadge->setText(statusText);
    mLblStatusBadge->setProperty("repoStatus", statusProp);
    mLblStatusBadge->style()->unpolish(mLblStatusBadge);
    mLblStatusBadge->style()->polish(mLblStatusBadge);

    // Metadata
    mLblStatus->setText(statusText);
    mLblStatus->setProperty("repoStatus", statusProp);
    mLblStatus->style()->unpolish(mLblStatus);
    mLblStatus->style()->polish(mLblStatus);

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

    if (diagnoseResult)
        showDiagnoseResult(*diagnoseResult, mIssuesLayout);

    show();
}

void RepoDetailPanel::addIssueWidget(const RepoHealthIssue &issue)
{
    QString severityProp;
    switch (issue.severity) {
    case RepoHealthIssue::Error:   severityProp = "error"; break;
    case RepoHealthIssue::Warning: severityProp = "warning"; break;
    default:                       severityProp = "info"; break;
    }

    QWidget *issueWidget = new QWidget(mIssuesContainer);
    issueWidget->setObjectName("repoIssueCard");
    issueWidget->setAttribute(Qt::WA_StyledBackground, true);
    issueWidget->setProperty("issueSeverity", severityProp);

    QVBoxLayout *issueLayout = new QVBoxLayout(issueWidget);
    issueLayout->setContentsMargins(12, 8, 10, 8);
    issueLayout->setSpacing(4);

    QLabel *lblSummary = new QLabel(issue.summary, issueWidget);
    lblSummary->setObjectName("repoIssueSummary");
    lblSummary->setProperty("issueSeverity", severityProp);
    issueLayout->addWidget(lblSummary);

    if (!issue.detail.isEmpty()) {
        QLabel *lblDetail = new QLabel(issue.detail, issueWidget);
        lblDetail->setObjectName("repoIssueDetail");
        lblDetail->setWordWrap(true);
        issueLayout->addWidget(lblDetail);
    }

    if (!issue.actions.isEmpty()) {
        QHBoxLayout *actionRow = new QHBoxLayout();
        actionRow->setSpacing(6);

        for (const RepoRepairAction &action : issue.actions) {
            QPushButton *btn = new QPushButton(action.label, issueWidget);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(26);

            if (action.type == RepoRepairAction::RemoveSource)
                btn->setObjectName("repoRemoveBtn");
            else
                btn->setAccessibleName("primary");

            RepoRepairAction capturedAction = action;
            connect(btn, &QPushButton::clicked, this, [this, capturedAction]() {
                emit repairActionRequested(capturedAction, mCurrentSource);
            });
            actionRow->addWidget(btn);
        }
        actionRow->addStretch();
        issueLayout->addLayout(actionRow);
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

void RepoDetailPanel::showDiagnoseResult(const DiagnoseResult &result, QVBoxLayout *targetLayout)
{
    for (const DiagnoseStep &step : result.steps) {
        QHBoxLayout *stepRow = new QHBoxLayout();
        stepRow->setSpacing(6);

        QString icon, diagStatus;
        switch (step.status) {
        case DiagnoseStep::Ok:
            icon = QString::fromUtf8("\u2713"); diagStatus = "ok"; break;
        case DiagnoseStep::Warning:
            icon = QString::fromUtf8("\u25B2"); diagStatus = "warning"; break;
        case DiagnoseStep::Failed:
            icon = QString::fromUtf8("\u2717"); diagStatus = "failed"; break;
        }

        QLabel *lblIcon = new QLabel(icon);
        lblIcon->setObjectName("repoDiagIcon");
        lblIcon->setProperty("diagStatus", diagStatus);
        lblIcon->setFixedWidth(16);
        stepRow->addWidget(lblIcon);

        QLabel *lblCheck = new QLabel(QString("<b>%1:</b> %2").arg(step.check, step.detail));
        lblCheck->setObjectName("repoDiagnoseStep");
        lblCheck->setWordWrap(true);
        stepRow->addWidget(lblCheck, 1);

        targetLayout->insertLayout(targetLayout->count() - 1, stepRow);
    }

    if (!result.suggestion.isEmpty()) {
        QLabel *lblSuggestion = new QLabel(result.suggestion);
        lblSuggestion->setObjectName("repoSuggestion");
        lblSuggestion->setWordWrap(true);
        targetLayout->insertWidget(targetLayout->count() - 1, lblSuggestion);
    }

    if (!result.followUpActions.isEmpty()) {
        QHBoxLayout *actionRow = new QHBoxLayout();
        actionRow->setSpacing(6);
        for (const RepoRepairAction &action : result.followUpActions) {
            QPushButton *btn = new QPushButton(action.label);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(26);

            if (action.type == RepoRepairAction::DisableSource)
                btn->setAccessibleName("primary");

            RepoRepairAction capturedAction = action;
            connect(btn, &QPushButton::clicked, this, [this, capturedAction]() {
                emit repairActionRequested(capturedAction, mCurrentSource);
            });
            actionRow->addWidget(btn);
        }
        actionRow->addStretch();
        targetLayout->insertLayout(targetLayout->count() - 1, actionRow);
    }
}

void RepoDetailPanel::refreshThemeColors()
{
    if (mCurrentSource) {
        showRepo(mCurrentSource, mCurrentResult);
    }
}
