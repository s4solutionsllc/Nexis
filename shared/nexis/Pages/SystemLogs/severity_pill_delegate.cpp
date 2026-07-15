#include "severity_pill_delegate.h"
#include "Managers/app_manager.h"

#include <QPainter>
#include <QSettings>

namespace {

QColor styleColor(QSettings *sv, const char *token, const char *fallback)
{
    return QColor(sv ? sv->value(QLatin1String(token), QLatin1String(fallback)).toString()
                      : QLatin1String(fallback));
}

// Maps syslog severity (0=Emergency ... 7=Debug) to a DS §5 status color.
// NOTICE -> info, INFO -> dimmed are the two severities in the approved
// capture; error/warning are wired the same way so they render correctly
// the moment a live log stream includes them, with no further code changes.
QColor colorForSeverity(int severity, QSettings *sv)
{
    if (severity <= 3)
        return styleColor(sv, "@destructiveColor", "#E05454");
    if (severity == 4)
        return styleColor(sv, "@warningColor", "#FFB347");
    if (severity == 5)
        return styleColor(sv, "@infoColor", "#5B9BD5");
    return styleColor(sv, "@tertiaryText", "#8A8A8A");
}

} // namespace

SeverityPillDelegate::SeverityPillDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void SeverityPillDelegate::initStyleOption(QStyleOptionViewItem *option,
                                            const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->text.clear(); // suppress default text draw; paint() draws the pill instead
}

void SeverityPillDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    // Background only (selection/hover/zebra), text blanked via initStyleOption above.
    QStyledItemDelegate::paint(painter, option, index);

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty())
        return;

    QSettings *sv = AppManager::ins()->getStyleValues();
    const QColor track = styleColor(sv, "@chartGridColor", "#E5E5E5");
    const QColor textColor = colorForSeverity(index.data(Qt::UserRole).toInt(), sv);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QFont font = option.font;
    font.setPointSize(8);
    painter->setFont(font);

    QFontMetrics fm(font);
    const int pillHeight = 18;
    const int pillWidth = qMax(44, fm.horizontalAdvance(text) + 16);
    const QRect pillRect(option.rect.left() + 6,
                          option.rect.top() + (option.rect.height() - pillHeight) / 2,
                          pillWidth, pillHeight);

    painter->setPen(Qt::NoPen);
    painter->setBrush(track);
    painter->drawRoundedRect(pillRect, pillHeight / 2, pillHeight / 2);

    painter->setPen(textColor);
    painter->drawText(pillRect, Qt::AlignCenter, text);

    painter->restore();
}
