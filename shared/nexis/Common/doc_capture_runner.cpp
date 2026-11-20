#include "doc_capture_runner.h"

#include "doc_capture_frame.h"
#include "command_palette.h"
#include "signal_mapper.h"
#include "sliding_stacked_widget.h"
#include "Managers/app_manager.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QTableView>
#include <QTimer>
#include <QTreeView>

#include <cstdio>

namespace {

QColor themeColor(const char *token)
{
    QSettings *values = AppManager::ins()->getStyleValues();
    return values ? QColor(values->value(QLatin1String(token)).toString()) : QColor();
}

DocCaptureFrame::Style frameStyle()
{
    DocCaptureFrame::Style s;
    s.titleBar = themeColor("@sidebar");
    s.titleText = themeColor("@textSecondary");
    s.border = themeColor("@borderColor");
    s.shadow = themeColor("@shadowColor");
    s.closeButton = themeColor("@dangerFill");
    s.minButton = themeColor("@warningColor");
    s.maxButton = themeColor("@successColor");
#ifdef Q_OS_MACOS
    s.layout = DocCaptureFrame::ButtonLayout::MacOS;
#else
    s.layout = DocCaptureFrame::ButtonLayout::Linux;
#endif
    return s;
}

void log(const QString &line)
{
    std::fprintf(stdout, "%s\n", qPrintable(line));
    std::fflush(stdout);
}

bool isVisibleOnPage(QWidget *w)
{
    return w && w->isVisible() && w->isEnabled();
}

} // namespace

DocCaptureRunner::DocCaptureRunner(QWidget *mainWindow, const QList<DocCaptureShot> &shots,
                                   const Options &options, QObject *parent)
    : QObject(parent), mWindow(mainWindow), mShots(shots), mOptions(options)
{
}

void DocCaptureRunner::start()
{
    mUptime.start();
    // Size before the first show: pages that lay out on first show (the
    // Dashboard grid) would otherwise pack for the default window size and,
    // with no saved layout in a fresh profile, keep that packing.
    mWindow->resize(mOptions.windowSize);
    mWindow->showNormal();
    mWindow->resize(mOptions.windowSize);
    log(QStringLiteral("nexis --capture-docs: %1 shot(s) → %2")
            .arg(mShots.size()).arg(QDir(mOptions.outputRoot).absolutePath()));
    // Give the first page and DataRefreshService a moment before the first shot.
    QTimer::singleShot(2000, this, &DocCaptureRunner::nextShot);
}

void DocCaptureRunner::nextShot()
{
    ++mIndex;
    if (mIndex >= mShots.size()) {
        finish();
        return;
    }
    const DocCaptureShot &shot = mShots.at(mIndex);
    log(QStringLiteral("[%1/%2] %3").arg(mIndex + 1).arg(mShots.size()).arg(shot.file));

    if (!shot.page.isEmpty())
        emit SignalMapper::ins()->sigNavigateToPage(shot.page);

    QTimer::singleShot(static_cast<int>(shot.settle * 1000), this, [this]() {
        const DocCaptureShot &s = mShots.at(mIndex);
        runSteps(s.steps, 0, [this]() { capture(); });
    });
}

void DocCaptureRunner::runSteps(const QList<DocCaptureStep> &steps, int index,
                                std::function<void()> done)
{
    if (index >= steps.size()) {
        done();
        return;
    }
    runStep(steps.at(index), [this, steps, index, done]() {
        // A short pause between steps lets the UI react (animations, lazy loads).
        QTimer::singleShot(300, this, [this, steps, index, done]() {
            runSteps(steps, index + 1, done);
        });
    });
}

void DocCaptureRunner::runStep(const DocCaptureStep &step, std::function<void()> done)
{
    using T = DocCaptureStep::Type;
    switch (step.type) {
    case T::Click: {
        QWidget *w = step.object.isEmpty() ? findButtonByText(step.text) : findWidget(step.object);
        auto *btn = qobject_cast<QAbstractButton*>(w);
        if (!btn) {
            fail(QStringLiteral("click: no button \"%1\"").arg(step.object.isEmpty() ? step.text : step.object));
        } else {
            btn->click();
        }
        done();
        return;
    }
    case T::SetText: {
        QWidget *w = findWidget(step.object);
        if (auto *edit = qobject_cast<QLineEdit*>(w)) {
            edit->setText(step.text);
        } else if (auto *combo = qobject_cast<QComboBox*>(w); combo && combo->isEditable()) {
            combo->setEditText(step.text);
        } else {
            fail(QStringLiteral("setText: no text field \"%1\"").arg(step.object));
        }
        done();
        return;
    }
    case T::Wait:
        QTimer::singleShot(static_cast<int>(step.seconds * 1000), this, done);
        return;
    case T::WaitUptime: {
        const qint64 remaining = static_cast<qint64>(step.seconds * 1000) - mUptime.elapsed();
        QTimer::singleShot(static_cast<int>(qMax<qint64>(0, remaining)), this, done);
        return;
    }
    case T::WaitEnabled: {
        auto *timer = new QTimer(this);
        auto *clock = new QElapsedTimer;
        clock->start();
        const QString name = step.object;
        const qint64 limit = static_cast<qint64>(step.seconds * 1000);
        connect(timer, &QTimer::timeout, this, [this, timer, clock, name, limit, done]() {
            QWidget *w = findWidget(name);
            const bool ready = w && w->isEnabled();
            if (ready || clock->elapsed() > limit) {
                if (!ready)
                    fail(QStringLiteral("waitEnabled: \"%1\" not enabled after %2 s").arg(name).arg(limit / 1000));
                timer->stop();
                timer->deleteLater();
                delete clock;
                done();
            }
        });
        timer->start(250);
        return;
    }
    case T::Scroll: {
        QWidget *page = currentPage();
        QAbstractScrollArea *best = nullptr;
        if (page) {
            for (QAbstractScrollArea *a : page->findChildren<QAbstractScrollArea*>()) {
                if (!a->isVisible() || !a->verticalScrollBar() || a->verticalScrollBar()->maximum() == 0)
                    continue;
                if (!best || a->height() > best->height())
                    best = a;
            }
        }
        if (best) {
            QScrollBar *bar = best->verticalScrollBar();
            bar->setValue(static_cast<int>(bar->minimum() + step.position * (bar->maximum() - bar->minimum())));
        } else {
            fail(QStringLiteral("scroll: no scrollable area on the page"));
        }
        done();
        return;
    }
    case T::ExpandTrees: {
        if (QWidget *page = currentPage()) {
            for (QTreeView *tv : page->findChildren<QTreeView*>()) {
                if (tv->isVisible())
                    tv->expandToDepth(0);
            }
        }
        done();
        return;
    }
    case T::Sort: {
        bool sorted = false;
        if (QWidget *page = currentPage()) {
            for (QAbstractItemView *view : page->findChildren<QAbstractItemView*>()) {
                if (!view->isVisible() || !view->model())
                    continue;
                QHeaderView *header = nullptr;
                if (auto *tv = qobject_cast<QTreeView*>(view))
                    header = tv->header();
                else if (auto *tab = qobject_cast<QTableView*>(view))
                    header = tab->horizontalHeader();
                if (!header)
                    continue;
                for (int c = 0; c < view->model()->columnCount(); ++c) {
                    const QString label = view->model()->headerData(c, Qt::Horizontal).toString().trimmed();
                    if (label.compare(step.text, Qt::CaseInsensitive) == 0) {
                        header->setSortIndicator(c, step.descending ? Qt::DescendingOrder : Qt::AscendingOrder);
                        view->model()->sort(c, step.descending ? Qt::DescendingOrder : Qt::AscendingOrder);
                        sorted = true;
                        break;
                    }
                }
                if (sorted)
                    break;
            }
        }
        if (!sorted)
            fail(QStringLiteral("sort: no column \"%1\"").arg(step.text));
        done();
        return;
    }
    case T::ShowPalette: {
        if (auto *palette = mWindow->findChild<CommandPalette*>())
            palette->show();
        else
            fail(QStringLiteral("showPalette: command palette not found"));
        done();
        return;
    }
    case T::Key: {
        const QKeySequence seq(step.text);
        QWidget *target = QApplication::activePopupWidget();
        if (!target)
            target = QApplication::focusWidget();
        if (!target)
            target = mWindow;
        if (seq.isEmpty()) {
            fail(QStringLiteral("key: cannot parse \"%1\"").arg(step.text));
        } else {
            const QKeyCombination combo = seq[0];
            QKeyEvent press(QEvent::KeyPress, combo.key(), combo.keyboardModifiers());
            QKeyEvent release(QEvent::KeyRelease, combo.key(), combo.keyboardModifiers());
            QCoreApplication::sendEvent(target, &press);
            QCoreApplication::sendEvent(target, &release);
        }
        done();
        return;
    }
    case T::CloseDialog: {
        if (QWidget *dlg = findDialog(step.text))
            dlg->close();
        done();
        return;
    }
    }
    done();
}

void DocCaptureRunner::capture()
{
    const DocCaptureShot &shot = mShots.at(mIndex);

    QWidget *target = mWindow;
    QString title = mWindow->windowTitle();
    if (!shot.dialog.isEmpty()) {
        target = findDialog(shot.dialog);
        if (!target) {
            fail(QStringLiteral("no visible window titled \"%1\"").arg(shot.dialog));
        } else {
            title = target->windowTitle();
        }
    }

    if (target) {
        QImage image = shot.dialog.isEmpty() ? grabWithPopups(target) : target->grab().toImage();
        const int blurred = blurWidgets(target, shot.blur, &image);
        if (!shot.blur.isEmpty() && blurred == 0)
            fail(QStringLiteral("blur: none of %1 visible").arg(shot.blur.join(", ")));
        const bool fullScreen = target->isFullScreen();
        if (mOptions.frame && shot.frame && !fullScreen)
            image = DocCaptureFrame::decorate(image, title, frameStyle());

        const QString path = QDir(mOptions.outputRoot).filePath(shot.file);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QImage out = image;
        out.setDevicePixelRatio(1.0);
        if (out.save(path, "PNG")) {
            mWritten << shot.file;
            log(QStringLiteral("    wrote %1 (%2×%3)").arg(path).arg(out.width()).arg(out.height()));
        } else {
            fail(QStringLiteral("could not write %1").arg(path));
        }
    }

    runSteps(shot.after, 0, [this]() {
        QTimer::singleShot(500, this, &DocCaptureRunner::nextShot);
    });
}

void DocCaptureRunner::finish()
{
    log(QStringLiteral("nexis --capture-docs: wrote %1, failed %2").arg(mWritten.size()).arg(mFailed.size()));
    for (const QString &f : mFailed)
        log(QStringLiteral("  FAILED %1").arg(f));
    QCoreApplication::exit(mFailed.isEmpty() ? 0 : 1);
}

QWidget *DocCaptureRunner::currentPage() const
{
    auto *stack = mWindow->findChild<SlidingStackedWidget*>();
    return stack ? stack->currentWidget() : nullptr;
}

QWidget *DocCaptureRunner::findWidget(const QString &objectName) const
{
    // Prefer a visible match on the current page, then anywhere in the main
    // window, then in any other visible top-level window (dialogs, popups).
    QList<QWidget*> scopes;
    if (QWidget *page = currentPage())
        scopes << page;
    scopes << mWindow;
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (top != mWindow && top->isVisible())
            scopes << top;
    }
    for (QWidget *scope : scopes) {
        if (scope->objectName() == objectName && scope->isVisible())
            return scope;
        for (QWidget *w : scope->findChildren<QWidget*>(objectName)) {
            if (w->isVisible())
                return w;
        }
    }
    return nullptr;
}

QWidget *DocCaptureRunner::findButtonByText(const QString &text) const
{
    auto matches = [&text](QAbstractButton *b) {
        QString t = b->text();
        t.remove(QLatin1Char('&'));
        return isVisibleOnPage(b) && t.trimmed().compare(text, Qt::CaseInsensitive) == 0;
    };
    if (QWidget *page = currentPage()) {
        for (QAbstractButton *b : page->findChildren<QAbstractButton*>())
            if (matches(b))
                return b;
    }
    for (QAbstractButton *b : mWindow->findChildren<QAbstractButton*>())
        if (matches(b))
            return b;
    return nullptr;
}

QWidget *DocCaptureRunner::findDialog(const QString &title) const
{
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (top == mWindow || !top->isVisible())
            continue;
        if (top->windowTitle().contains(title, Qt::CaseInsensitive) || top->objectName() == title)
            return top;
    }
    return nullptr;
}

int DocCaptureRunner::blurWidgets(QWidget *window, const QStringList &names, QImage *image) const
{
    const qreal dpr = image->devicePixelRatio() > 0 ? image->devicePixelRatio() : 1.0;
    int count = 0;
    for (const QString &name : names) {
        for (QWidget *w : window->findChildren<QWidget*>(name)) {
            if (!w->isVisible())
                continue;
            // visibleRegion() is already clipped by scroll viewports and
            // parents, so rows scrolled out of view are skipped.
            const QRect visible = w->visibleRegion().boundingRect();
            if (visible.isEmpty())
                continue;
            const QRect inWindow(w->mapTo(window, visible.topLeft()), visible.size());
            const QRect padded = inWindow.adjusted(-2, -2, 2, 2);
            DocCaptureFrame::blurRect(*image, QRect(qRound(padded.x() * dpr), qRound(padded.y() * dpr),
                                                    qRound(padded.width() * dpr), qRound(padded.height() * dpr)));
            ++count;
        }
    }
    return count;
}

QImage DocCaptureRunner::grabWithPopups(QWidget *window) const
{
    QImage base = window->grab().toImage();
    const qreal dpr = base.devicePixelRatio() > 0 ? base.devicePixelRatio() : 1.0;
    const QRect winRect(window->mapToGlobal(QPoint(0, 0)), window->size());

    // Popups (command palette, menus) are separate top-level windows; paint
    // the ones over the main window into the capture at their position.
    QPainter p(&base);
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (top == window || !top->isVisible())
            continue;
        const Qt::WindowType type = top->windowType();
        if (type != Qt::Popup && type != Qt::ToolTip && type != Qt::Tool)
            continue;
        const QRect r(top->mapToGlobal(QPoint(0, 0)), top->size());
        if (!r.intersects(winRect))
            continue;
        QImage overlay = top->grab().toImage();
        overlay.setDevicePixelRatio(dpr);
        p.drawImage(QPointF(r.topLeft() - winRect.topLeft()), overlay);
    }
    p.end();
    return base;
}

void DocCaptureRunner::fail(const QString &reason)
{
    const QString file = (mIndex >= 0 && mIndex < mShots.size()) ? mShots.at(mIndex).file : QString();
    const QString entry = file.isEmpty() ? reason : QStringLiteral("%1: %2").arg(file, reason);
    if (!mFailed.contains(entry))
        mFailed << entry;
    log(QStringLiteral("    ! %1").arg(reason));
}
