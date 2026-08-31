#ifndef SNAPSHOT_MANAGER_WIDGET_H
#define SNAPSHOT_MANAGER_WIDGET_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;

// SSO-23867: macOS APFS/Time Machine local snapshot manager — list, create,
// and delete local snapshots via `tmutil`, and surface the disk space they
// hold purgeable so users can see the effect of deleting them. Part of the
// SSO-15367 macOS Power Toolkit epic.
//
// Deliberately independent of Services/snapshot_service.* — that class is
// FR-112's silent pre-clean safety snapshot (a one-way `tmutil localsnapshot`
// write with no listing/deletion), not a management surface. Do not merge
// the two.
class SnapshotManagerWidget : public QWidget
{
    Q_OBJECT

public:
    // One local snapshot as reported by `tmutil listlocalsnapshots /`.
    struct SnapshotEntry {
        QString dateToken;    // e.g. "2024-01-15-120000" — the deletelocalsnapshots argument
        QDateTime timestamp;  // parsed from dateToken, for display + sort order

        bool operator==(const SnapshotEntry &other) const
        {
            return dateToken == other.dateToken && timestamp == other.timestamp;
        }
    };

    struct State {
        bool available = false;           // tmutil present on this system
        QString errorMsg;
        QList<SnapshotEntry> snapshots;   // newest first
        qint64 availableBytes = -1;       // purgeable-inclusive free space on "/", -1 if unknown
    };

    explicit SnapshotManagerWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

    // Pure, platform-independent parsing exercised directly by unit tests
    // (SSO-23867 AC: no live filesystem required in CI). Tolerant of the
    // optional "Snapshots for ...:" header line tmutil prepends on newer
    // macOS releases, and of blank/unrelated lines.
    static QList<SnapshotEntry> parseListLocalSnapshots(const QString &output);

signals:
    void stateFetched(SnapshotManagerWidget::State state);

private slots:
    void onStateFetched(SnapshotManagerWidget::State state);
    void onCreateClicked();
    void onDeleteClicked(const QString &dateToken);
    void refreshThemeColors();

private:
    State fetchState() const;
    void buildUI();
    void renderState(const State &state);
    void rebuildSnapshotRows();
    void setBusy(bool busy);

    QLabel      *mLblTitle      = nullptr;
    QFrame      *mCard          = nullptr;
    QLabel      *mLblAvailable  = nullptr;
    QPushButton *mBtnCreate     = nullptr;
    QPushButton *mBtnRefresh    = nullptr;
    QLabel      *mLblLoading    = nullptr;
    QLabel      *mLblResult     = nullptr;
    QLabel      *mLblEmpty      = nullptr;
    QWidget     *mListContainer = nullptr;
    QVBoxLayout *mListLayout    = nullptr;

    bool  mLoaded = false;
    State mCurrent;
};

#endif // SNAPSHOT_MANAGER_WIDGET_H
