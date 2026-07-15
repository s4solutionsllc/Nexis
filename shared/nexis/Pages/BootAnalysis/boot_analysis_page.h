#ifndef BOOT_ANALYSIS_PAGE_H
#define BOOT_ANALYSIS_PAGE_H

#include <QWidget>
#include <QAtomicInt>
#include <QFuture>

#include <Info/boot_analysis_info.h>

class QLabel;
class QTableWidget;
class QPushButton;
class InfoManager;

class BootAnalysisPage : public QWidget
{
    Q_OBJECT

public:
    explicit BootAnalysisPage(QWidget *parent = nullptr,
                              InfoManager *infoManager = nullptr);
    ~BootAnalysisPage() override;

private slots:
    void onRefresh();

private:
    void buildUi();
    void populate(const BootAnalysisData &data);

    QLabel        *mLblTitle       = nullptr;
    QLabel        *mLblSource      = nullptr;
    QTableWidget  *mTable          = nullptr;
    QPushButton   *mBtnRefresh     = nullptr;
    QLabel        *mLblStatus      = nullptr;

    // DS §2/§3/§4 (NEX-13744): elevated card promoting the uptime metric
    // off the bare page — accent bar + title + 14pt/700 hero value.
    QWidget       *mUptimeContainer = nullptr;
    QLabel        *mLblUptimeTitle  = nullptr;
    QLabel        *mLblUptimeValue  = nullptr;
    QLabel        *mLblUptimeMeta   = nullptr;

    // DS §5 (NEX-13744): upgrades the old text-only #lblBootEmptyState into
    // icon + explanation + "Refresh uptime" next-action, inside its own
    // elevated card.
    QWidget       *mEmptyContainer     = nullptr;
    QWidget       *mEmptyState         = nullptr;
    QLabel        *mLblEmptyHeading    = nullptr;
    QLabel        *mLblEmptyText       = nullptr;
    QPushButton   *mBtnRefreshUptime   = nullptr;

    InfoManager                       *mIm        = nullptr;
    QFuture<BootAnalysisData>          mFuture;
    QAtomicInt                         mCancelled{0};
};

#endif // BOOT_ANALYSIS_PAGE_H
