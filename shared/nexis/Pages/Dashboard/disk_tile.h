#ifndef DISK_TILE_H
#define DISK_TILE_H

#include <QWidget>
#include <QLabel>
#include <QColor>
#include <QPushButton>
#include <QHBoxLayout>
#include <functional>

class DiskTile : public QWidget
{
    Q_OBJECT

public:
    explicit DiskTile(const QString &arcColorToken, const QString &trackColorToken, QWidget *parent = nullptr);
    ~DiskTile() = default;

    void setValue(int percent, const QString &usedText, const QString &totalText);
    void setSubtitle(const QString &text);
    void setDriveHealth(const QString &driveName, const QString &status, bool healthy = true);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildLayout();
    void refreshThemeColors();

    QString mArcColorToken;
    QString mTrackColorToken;
    QColor mArcColor;
    QColor mTrackColor;
    QColor mTextColor;

    int mPercent;
    QString mUsedText;
    QString mTotalText;

    QLabel *mLblTitle;
    QLabel *mLblSubtitle;
    QWidget *mHealthContainer;
    QHBoxLayout *mHealthLayout;

    struct HealthEntry {
        QLabel *statusLabel;
        bool healthy;
    };
    QList<HealthEntry> mHealthEntries;
};

#endif // DISK_TILE_H
