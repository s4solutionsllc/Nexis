#ifndef CACHE_REBUILD_WIDGET_H
#define CACHE_REBUILD_WIDGET_H

#include <QList>
#include <QOperatingSystemVersion>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;

// SSO-23866: macOS granular cache rebuilds — Helpers-page widget/action per
// cache (dyld shared cache, XPC cache, font cache, Launchpad database),
// matching the confirm -> async run -> progress -> result pattern used by
// SwappinessWidget / TrimWidget rather than introducing a new UI surface.
// Part of the SSO-15367 macOS Power Toolkit epic.
class CacheRebuildWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Action {
        DyldSharedCache,
        XpcCache,
        FontCache,
        LaunchpadReset,
    };
    static constexpr int kActionCount = 4;

    // One command run as part of a rebuild sequence, in order.
    struct Step {
        QString cmd;
        QStringList args;
        bool needsSudo = false;
        QString description;   // shown in the progress label while this step runs
    };

    struct SupportInfo {
        bool available = true;
        QString reason;   // populated when !available — surfaced to the user
    };

    explicit CacheRebuildWidget(QWidget *parent = nullptr);

    // Pure, platform-independent logic exercised directly by unit tests
    // (SSO-23866 AC: no live macOS rebuild required in CI).
    static QList<Step> commandsFor(Action action);
    static SupportInfo supportInfo(Action action, QOperatingSystemVersion version);
    static QString actionTitle(Action action);
    static QString actionDescription(Action action);
    static QString confirmText(Action action);

private slots:
    void refreshThemeColors();

private:
    struct ActionRow {
        QFrame      *card        = nullptr;
        QLabel      *lblStatus   = nullptr;
        QPushButton *btnRun      = nullptr;
        QLabel      *lblProgress = nullptr;
        QLabel      *lblResult   = nullptr;
    };

    void buildUI();
    QFrame *buildRow(Action action);
    void runAction(Action action);
    void setRowRunning(Action action, bool running);
    // Combines the pure version gate with a runtime check that the first
    // command in the sequence actually exists on this machine.
    SupportInfo effectiveSupport(Action action) const;

    QVector<ActionRow> mRows;   // indexed by static_cast<int>(Action)
};

#endif // CACHE_REBUILD_WIDGET_H
