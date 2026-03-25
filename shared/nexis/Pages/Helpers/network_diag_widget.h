#ifndef NETWORK_DIAG_WIDGET_H
#define NETWORK_DIAG_WIDGET_H

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QPushButton;

struct DiagCheck {
    QString label;
    bool    passed   = false;
    double  latencyMs = -1.0;
    QString errorMsg;
};

struct DiagResult {
    QList<DiagCheck> checks;
    QStringList      dnsServers;
    QString          interface;
};

class NetworkDiagWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkDiagWidget(QWidget *parent = nullptr);

    void runTestIfNeeded();
    void runTest();

    // Static parsers (testable without widget instantiation)
    static QString parseGatewayFromRoute(const QString &output);
    static QString parseGatewayFromIpRoute(const QString &output);
    static double  parsePingLatency(const QString &output);
    static QStringList parseDnsFromScutilDns(const QString &output);
    static QStringList parseDnsFromResolvectl(const QString &output);
    static QStringList parseDnsFromResolvConf(const QString &output);

signals:
    void diagnosticsFinished(DiagResult result);

private slots:
    void onDiagnosticsFinished(DiagResult result);

private:
    DiagResult runDiagnostics();
    DiagCheck  pingHost(const QString &label, const QString &host);
    QString    discoverGateway();
    QStringList discoverDnsServers();

    void buildUI();
    void clearResults();

    QLabel      *mLblTitle       = nullptr;
    QVBoxLayout *mResultsLayout  = nullptr;
    QLabel      *mLblDnsHeader   = nullptr;
    QVBoxLayout *mDnsLayout      = nullptr;
    QLabel      *mLblInterface   = nullptr;
    QLabel      *mLblLoading     = nullptr;
    QPushButton *mBtnRetest      = nullptr;

    bool mHasRun = false;
};

#endif // NETWORK_DIAG_WIDGET_H
