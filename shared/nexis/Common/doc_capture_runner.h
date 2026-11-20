#ifndef DOC_CAPTURE_RUNNER_H
#define DOC_CAPTURE_RUNNER_H

#include <QElapsedTimer>
#include <QImage>
#include <functional>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>

#include "doc_capture_manifest.h"

class QWidget;
class QAbstractItemView;
class QHeaderView;

// Drives a live App window through the documentation screenshot manifest
// (`nexis --capture-docs <outdir>`): navigates to each page, runs the
// manifest steps, grabs the window with QWidget::grab() — which renders the
// widget tree itself, so it needs no screen-recording permission, works on
// Wayland and is unaffected by other windows covering Nexis — adds window
// chrome, and writes the PNG. Quits the application when done (exit code 0
// if every shot was written, 1 otherwise).
class DocCaptureRunner : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString outputRoot;          // files are written relative to this
        QSize windowSize{1320, 840}; // logical size; the default Dashboard layout needs 8 columns (>= ~1300 wide)
        bool frame = true;           // global switch for window chrome
    };

    DocCaptureRunner(QWidget *mainWindow, const QList<DocCaptureShot> &shots,
                     const Options &options, QObject *parent = nullptr);

    void start();

private:
    void nextShot();
    void runSteps(const QList<DocCaptureStep> &steps, int index, std::function<void()> done);
    void runStep(const DocCaptureStep &step, std::function<void()> done);
    void runInstantStep(const DocCaptureStep &step);
    void waitUntilEnabled(const QString &name, qint64 limitMs, std::function<void()> done);
    void stepClick(const DocCaptureStep &step);
    void stepSetText(const DocCaptureStep &step);
    void stepScroll(const DocCaptureStep &step);
    void stepExpandTrees();
    void stepSort(const DocCaptureStep &step);
    void stepShowColumn(const DocCaptureStep &step);
    void stepShowPalette();
    void stepKey(const DocCaptureStep &step);
    void capture();
    void finish();

    QWidget *currentPage() const;
    QWidget *findWidget(const QString &objectName) const;
    QWidget *findButtonByText(const QString &text) const;
    QWidget *findDialog(const QString &title) const;
    QImage grabWithPopups(QWidget *window) const;
    int blurWidgets(QWidget *window, const QStringList &names, QImage *image) const;
    int blurColumn(QWidget *window, const QString &label, QImage *image) const;
    void forEachColumn(const QString &label,
                       const std::function<void(QAbstractItemView*, QHeaderView*, int)> &fn,
                       bool includeHidden) const;
    void fail(const QString &reason);

    QWidget *mWindow;
    QList<DocCaptureShot> mShots;
    Options mOptions;
    int mIndex = -1;
    QElapsedTimer mUptime;
    QStringList mWritten;
    QStringList mFailed;
};

#endif // DOC_CAPTURE_RUNNER_H
