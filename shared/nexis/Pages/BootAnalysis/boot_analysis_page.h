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

    QLabel        *mLblTitle    = nullptr;
    QLabel        *mLblSubtitle = nullptr;
    QTableWidget  *mTable       = nullptr;
    QLabel        *mLblEmpty    = nullptr;
    QPushButton   *mBtnRefresh  = nullptr;
    QLabel        *mLblStatus   = nullptr;

    InfoManager                       *mIm        = nullptr;
    QFuture<BootAnalysisData>          mFuture;
    QAtomicInt                         mCancelled{0};
};

#endif // BOOT_ANALYSIS_PAGE_H
