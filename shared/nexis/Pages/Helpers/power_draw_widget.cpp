#include "power_draw_widget.h"

#include "signal_mapper.h"
#include "utilities.h"
#include <Managers/app_manager.h>

#include <QHBoxLayout>
#include <QLocale>
#include <QStyle>

PowerDrawWidget::PowerDrawWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme,
            this, &PowerDrawWidget::refreshThemeColors);
    refreshThemeColors();

    // Start hidden — the parent reveals us after the first snapshot lands
    // with `available == true`. Avoids a flash of an empty card on hosts
    // that turn out to have no RAPL zones at all.
    hide();
}

void PowerDrawWidget::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    mCard = new QFrame(this);
    mCard->setObjectName("powerDrawCard");
    root->addWidget(mCard);

    auto *card = new QVBoxLayout(mCard);
    card->setContentsMargins(16, 14, 16, 14);
    card->setSpacing(8);

    mAccentBar = new QFrame(mCard);
    mAccentBar->setObjectName("sectionHeaderAccent");
    mAccentBar->setFixedWidth(3);
    mAccentBar->setFrameShape(QFrame::NoFrame);
    mAccentBar->setVisible(false);

    mTitle = new QLabel(tr("Package Power"), mCard);
    mTitle->setObjectName("powerDrawTitle");
    QFont titleFont = mTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    mTitle->setFont(titleFont);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(mAccentBar);
    titleRow->addWidget(mTitle);
    titleRow->addStretch();
    card->addLayout(titleRow);

    mWattsLabel = new QLabel(mCard);
    mWattsLabel->setObjectName("powerDrawWatts");
    QFont wattsFont = mWattsLabel->font();
    wattsFont.setPointSize(wattsFont.pointSize() + 8);
    wattsFont.setBold(true);
    mWattsLabel->setFont(wattsFont);
    card->addWidget(mWattsLabel);

    mBreakdown = new QLabel(mCard);
    mBreakdown->setObjectName("powerDrawBreakdown");
    mBreakdown->setWordWrap(true);
    card->addWidget(mBreakdown);
}

void PowerDrawWidget::onPowerUpdated(const RaplPowerSnapshot &snap)
{
    if (!snap.available) {
        hide();
        return;
    }
    show();

    mWattsLabel->setText(tr("%1 W").arg(snap.totalPackageWatts, 0, 'f', 1));

    if (snap.packages.size() > 1) {
        QStringList parts;
        for (const RaplPackageSnapshot &pkg : snap.packages)
            parts << tr("%1: %2 W").arg(pkg.name).arg(pkg.watts, 0, 'f', 1);
        mBreakdown->setText(parts.join(QStringLiteral("  ·  ")));
        mBreakdown->show();
    } else {
        mBreakdown->hide();
    }
}

void PowerDrawWidget::refreshThemeColors()
{
    QSettings *sv = AppManager::ins()->getStyleValues();
    if (!sv || !mCard)
        return;

    const QString secondary = sv->value("@color06").toString();

    if (mBreakdown)
        mBreakdown->setStyleSheet(QString("color: %1;").arg(secondary));
}

void PowerDrawWidget::setElevated(const QString &accentToken)
{
    mCard->setAttribute(Qt::WA_StyledBackground, true);
    mCard->setProperty("cardRole", "elevated");
    mCard->style()->unpolish(mCard);
    mCard->style()->polish(mCard);

    mAccentBar->setProperty("accentToken", accentToken);
    mAccentBar->setVisible(true);
    mAccentBar->style()->unpolish(mAccentBar);
    mAccentBar->style()->polish(mAccentBar);

    Utilities::addDropShadow(mCard, 90, 26);
}
