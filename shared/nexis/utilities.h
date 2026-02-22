#ifndef UTILITIES_H
#define UTILITIES_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>
#include "Managers/app_manager.h"

class Utilities
{
public:
    static void
    addDropShadow(QWidget *widget, const int alpha, const int blur = 15)
    {
        addDropShadow(QList<QWidget*>() << widget, alpha, blur);
    }

    static void
    addDropShadow(QList<QWidget *> widgets, const int alpha, const int blur = 15)
    {
        QColor baseColor(0, 0, 0, alpha);
        QSettings *sv = AppManager::ins()->getStyleValues();
        if (sv) {
            baseColor = QColor(sv->value("@shadowColor").toString());
            baseColor.setAlpha(alpha);
        }

        for (QWidget *widget: widgets) {
            QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(widget);
            effect->setBlurRadius(blur);
            effect->setColor(baseColor);
            effect->setOffset(0, 2);
            widget->setGraphicsEffect(effect);
        }
    }

    static QString
    getDesktopValue(const QRegularExpression &val, const QStringList &lines)
    {
        QStringList filteredList = lines.filter(val);
        if (filteredList.count() > 0) {
            QString line = filteredList.first().trimmed();
            int eqPos = line.indexOf('=');
            if (eqPos != -1) {
                return line.mid(eqPos + 1).trimmed();
            }
        }
        return QString("");
    }
};

#endif // UTILITIES_H
