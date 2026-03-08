#ifndef OPEN_PORTS_WIDGET_H
#define OPEN_PORTS_WIDGET_H

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

signals:
    void connectionsFetched(QList<ConnectionEntry> entries);

private slots:
    void onConnectionsFetched(QList<ConnectionEntry> entries);
    void onFilterChanged(const QString &text);
    void onListenOnlyToggled(bool checked);
    void refreshThemeColors();

private:
    QList<ConnectionEntry> fetchConnections(bool listenOnly);
    void buildUI();

    QLabel               *mLblTitle      = nullptr;
    QLineEdit            *mTxtSearch     = nullptr;
    QPushButton          *mBtnListenOnly = nullptr;
    QPushButton          *mBtnRefresh    = nullptr;
    QTableView           *mTable         = nullptr;
    QStandardItemModel   *mModel         = nullptr;
    QSortFilterProxyModel *mProxy        = nullptr;
    QLabel               *mLblCount      = nullptr;
    QLabel               *mLblLoading    = nullptr;

    bool mLoaded = false;
};

#endif // OPEN_PORTS_WIDGET_H
