#ifndef OPEN_PORTS_WIDGET_H
#define OPEN_PORTS_WIDGET_H

#include <QHash>
#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QStandardItemModel;
class QSortFilterProxyModel;

struct ConnectionEntry {
    QString protocol;
    QString localAddress;
    int     localPort    = 0;
    QString remoteAddress;
    int     remotePort   = 0;
    int     pid          = -1;
    QString processName;
    QString state;

    // FR-121
    QString binaryPath;        // resolved full path; empty if unresolvable
    bool    isUnexpected = false;  // binary not under a trusted prefix
    bool    isUnsigned   = false;  // macOS only — set lazily by Verify Signatures
};

class OpenPortsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OpenPortsWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

    static QList<ConnectionEntry> parseLsofOutput(const QString &output);
    static QList<ConnectionEntry> parseSsOutput(const QString &output, const QString &protocol = "TCP");

    // FR-121: resolve the binary path for a PID. Linux reads
    // /proc/<pid>/exe; macOS calls proc_pidpath. Returns empty on
    // permission error. Pure (no member state).
    static QString resolveBinaryPath(int pid);

    // FR-121: true when `path` starts with any platform-trusted prefix
    // plus any user-configured extras (SettingKeys::TrustedBinderPrefixes).
    // Empty path returns true (nothing to flag).
    static bool isTrustedBinderPath(const QString &path);

signals:
    void connectionsFetched(QList<ConnectionEntry> entries);

private slots:
    void onConnectionsFetched(QList<ConnectionEntry> entries);
    void onFilterChanged(const QString &text);
    void onListenOnlyToggled(bool checked);
#ifdef Q_OS_MAC
    void onVerifySignatures();
#endif

private:
    QList<ConnectionEntry> fetchConnections(bool listenOnly);
    void buildUI();

    QLabel               *mLblTitle      = nullptr;
    QLineEdit            *mTxtSearch     = nullptr;
    QPushButton          *mBtnListenOnly = nullptr;
    QPushButton          *mBtnRefresh    = nullptr;
#ifdef Q_OS_MAC
    QPushButton          *mBtnVerifySigs = nullptr;
#endif
    QTableView           *mTable         = nullptr;
    QStandardItemModel   *mModel         = nullptr;
    QSortFilterProxyModel *mProxy        = nullptr;
    QLabel               *mLblCount      = nullptr;
    QLabel               *mLblLoading    = nullptr;

    bool mLoaded = false;

    // FR-121: codesign results cached by path. Populated on demand
    // when the user clicks Verify Signatures (macOS only).
    QHash<QString, bool> mCodesignCache;
    QList<ConnectionEntry> mEntries;   // latest fetched — used by Verify Signatures.
};

#endif // OPEN_PORTS_WIDGET_H
