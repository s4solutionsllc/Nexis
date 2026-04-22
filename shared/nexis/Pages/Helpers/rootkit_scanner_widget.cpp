#include "rootkit_scanner_widget.h"

#ifdef Q_OS_LINUX

#include "signal_mapper.h"
#include <Managers/app_manager.h>
#include <Utils/command_util.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

RootKitScannerWidget::RootKitScannerWidget(QWidget *parent)
    : QWidget(parent)
{
    if (CommandUtil::isExecutable("chkrootkit"))
        mTool = QStringLiteral("chkrootkit");
    else if (CommandUtil::isExecutable("rkhunter"))
        mTool = QStringLiteral("rkhunter");

    buildUI();

    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &RootKitScannerWidget::refreshThemeColors);
    refreshThemeColors();
}

RootKitScannerWidget::~RootKitScannerWidget()
{
    if (mProcess && mProcess->state() != QProcess::NotRunning) {
        mProcess->kill();
        mProcess->waitForFinished(1000);
    }
}

void RootKitScannerWidget::loadIfNeeded()
{
    if (!mLoaded) {
        mLoaded = true;
        setState(ScanState::Idle);
    }
}

void RootKitScannerWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    mLblTitle = new QLabel(tr("Rootkit Scanner"), this);
    QFont f   = mLblTitle->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    mLblTitle->setFont(f);
    root->addWidget(mLblTitle);

    mLblIntro = new QLabel(
        tr("Scan this system for rootkits using %1. "
           "Elevation (pkexec) is required to run the scan.")
            .arg(mTool.isEmpty() ? tr("the installed scanner") : mTool),
        this);
    mLblIntro->setWordWrap(true);
    root->addWidget(mLblIntro);

    mCard = new QFrame(this);
    mCard->setObjectName("rootkitCard");
    auto *cardLayout = new QVBoxLayout(mCard);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(8);

    mOutput = new QPlainTextEdit(mCard);
    mOutput->setReadOnly(true);
    mOutput->setMaximumHeight(220);
    mOutput->setPlaceholderText(tr("Scan output will appear here…"));
    QFont mono = mOutput->font();
    mono.setFamily(QStringLiteral("monospace"));
    mOutput->setFont(mono);
    mOutput->hide();
    cardLayout->addWidget(mOutput);

    mLblSummary = new QLabel(mCard);
    mLblSummary->setWordWrap(true);
    mLblSummary->hide();
    cardLayout->addWidget(mLblSummary);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    mBtnScan = new QPushButton(tr("Run Scan"), mCard);
    mBtnScan->setAccessibleName("primary");
    mBtnScan->setCursor(Qt::PointingHandCursor);
    connect(mBtnScan, &QPushButton::clicked, this, &RootKitScannerWidget::onScanClicked);
    btnRow->addWidget(mBtnScan);

    mBtnCancel = new QPushButton(tr("Cancel"), mCard);
    mBtnCancel->setCursor(Qt::PointingHandCursor);
    mBtnCancel->hide();
    connect(mBtnCancel, &QPushButton::clicked, this, &RootKitScannerWidget::onCancelClicked);
    btnRow->addWidget(mBtnCancel);

    btnRow->addStretch();
    cardLayout->addLayout(btnRow);

    root->addWidget(mCard);
    root->addStretch();
}

void RootKitScannerWidget::setState(ScanState s)
{
    mState = s;
    switch (s) {
    case ScanState::Idle:
        mBtnScan->setText(tr("Run Scan"));
        mBtnScan->setEnabled(!mTool.isEmpty());
        mBtnScan->show();
        mBtnCancel->hide();
        mOutput->hide();
        mLblSummary->hide();
        break;
    case ScanState::Scanning:
        mBtnScan->hide();
        mBtnCancel->show();
        mOutput->show();
        mOutput->clear();
        mLblSummary->hide();
        mFullOutput.clear();
        mLineBuffer.clear();
        break;
    case ScanState::Done:
    case ScanState::Error:
        mBtnScan->setText(tr("Run Again"));
        mBtnScan->setEnabled(true);
        mBtnScan->show();
        mBtnCancel->hide();
        mOutput->show();
        mLblSummary->show();
        break;
    }
}

void RootKitScannerWidget::onScanClicked()
{
    if (mTool.isEmpty())
        return;

    setState(ScanState::Scanning);

    const QString toolPath = QStandardPaths::findExecutable(mTool);
    QStringList args;
    if (mTool == QLatin1String("rkhunter"))
        args << QStringLiteral("--check") << QStringLiteral("--sk") << QStringLiteral("--nocolors");

    mProcess = new QProcess(this);
    mProcess->setProgram(QStringLiteral("pkexec"));
    mProcess->setArguments(QStringList{toolPath} + args);

    connect(mProcess, &QProcess::readyReadStandardOutput,
            this, &RootKitScannerWidget::onReadyRead);
    connect(mProcess, &QProcess::readyReadStandardError,
            this, &RootKitScannerWidget::onReadyReadStderr);
    connect(mProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus status) {
                onScanFinished(code, static_cast<int>(status));
            });

    mProcess->start();
}

void RootKitScannerWidget::onCancelClicked()
{
    if (mProcess) {
        mProcess->kill();
        mProcess->waitForFinished(1000);
        mProcess->deleteLater();
        mProcess = nullptr;
    }
    mLblSummary->setText(tr("Scan cancelled."));
    setState(ScanState::Idle);
    mLblSummary->show();
}

void RootKitScannerWidget::onReadyRead()
{
    if (!mProcess)
        return;
    mLineBuffer += QString::fromLocal8Bit(mProcess->readAllStandardOutput());
    int nl;
    while ((nl = mLineBuffer.indexOf('\n')) != -1) {
        const QString line = mLineBuffer.left(nl);
        mLineBuffer        = mLineBuffer.mid(nl + 1);
        mOutput->appendPlainText(line);
        mFullOutput += line + '\n';
    }
}

void RootKitScannerWidget::onReadyReadStderr()
{
    if (!mProcess)
        return;
    mLineBuffer += QString::fromLocal8Bit(mProcess->readAllStandardError());
    int nl;
    while ((nl = mLineBuffer.indexOf('\n')) != -1) {
        const QString line = mLineBuffer.left(nl);
        mLineBuffer        = mLineBuffer.mid(nl + 1);
        mOutput->appendPlainText(line);
        mFullOutput += line + '\n';
    }
}

void RootKitScannerWidget::onScanFinished(int exitCode, int /*exitStatus*/)
{
    // Flush any remaining buffered output
    if (!mLineBuffer.isEmpty()) {
        mOutput->appendPlainText(mLineBuffer);
        mFullOutput += mLineBuffer;
        mLineBuffer.clear();
    }

    mProcess->deleteLater();
    mProcess = nullptr;

    bool issuesFound = false;
    if (mTool == QLatin1String("chkrootkit")) {
        issuesFound = mFullOutput.contains(QLatin1String(" INFECTED"));
    } else {
        // rkhunter always exits 0; check warning count
        static const QRegularExpression re(QStringLiteral(R"(Warnings found during.+?:\s*(\d+))"));
        const QRegularExpressionMatch m = re.match(mFullOutput);
        if (m.hasMatch())
            issuesFound = (m.captured(1).toInt() > 0);
    }

    if (exitCode != 0 && mTool == QLatin1String("chkrootkit")) {
        issuesFound = true;
    }

    QSettings *sv = AppManager::ins()->getStyleValues();
    if (issuesFound) {
        const QString warn = sv->value("@warningColor", "#e67e22").toString();
        mLblSummary->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(warn));
        mLblSummary->setText(tr("⚠ Issues detected — review the output above."));
        setState(ScanState::Done);
    } else {
        const QString ok = sv->value("@successColor", "#27ae60").toString();
        mLblSummary->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(ok));
        mLblSummary->setText(tr("✓ No issues found."));
        setState(ScanState::Done);
    }
    mLblSummary->show();
}

void RootKitScannerWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    const QString cardBg     = sv->value("@cardBg",     "#ffffff").toString();
    const QString borderCol  = sv->value("@borderColor","#e0e0e0").toString();

    mCard->setStyleSheet(
        QStringLiteral("QFrame#rootkitCard{"
                       "background-color:%1;"
                       "border:1px solid %2;"
                       "border-radius:8px;}")
            .arg(cardBg, borderCol));
}

#endif // Q_OS_LINUX
