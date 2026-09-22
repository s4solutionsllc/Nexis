#ifndef UTILITIES_H
#define UTILITIES_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>
#include <QIcon>
#include <QPainter>
#include <QLabel>
#include "Managers/app_manager.h"
#include "dpi.h"
#include "signal_mapper.h"

class Utilities
{
public:
    // True if the user has requested reduced motion in the OS accessibility
    // settings; callers should skip decorative animations when set. Defined
    // in utilities.cpp (rather than inline here) because the macOS
    // implementation needs macos_window_helper.h, which only nexis-gui links
    // against — several test targets compile files that include this header
    // without linking nexis-gui.
    static bool prefersReducedMotion();

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

    // Accent-coloured glyphs in themes/common are authored in the Light accent
    // (#E95420). Re-colour them to the active theme's @accentColor so they match
    // the rest of the chrome in Dark as well.
    static QIcon
    accentIcon(const QString &svgPath, int logicalSize = 20)
    {
        const QIcon source(svgPath);
        QSettings *sv = AppManager::ins()->getStyleValues();
        const QColor accent(sv ? sv->value("@accentColor").toString() : QString());
        if (!accent.isValid())
            return source;

        // These glyphs are single-colour, so a SourceIn fill re-colours them
        // while keeping their anti-aliased alpha.
        QIcon icon;
        for (int scale : {1, 2, 3}) {
            QPixmap pm = source.pixmap(QSize(logicalSize, logicalSize) * scale);
            if (pm.isNull())
                continue;
            QPainter painter(&pm);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pm.rect(), accent);
            painter.end();
            pm.setDevicePixelRatio(scale);
            icon.addPixmap(pm);
        }
        return icon.isNull() ? source : icon;
    }

    // Empty states use the monochrome, per-theme sidebar icon set rather than
    // colour emoji, so they match the rest of the chrome in both themes.
    static void
    setEmptyStateIcon(QLabel *label, const QString &sidebarIconFile)
    {
        auto apply = [label, sidebarIconFile]() {
            const QString path = QStringLiteral(":/static/themes/%1/img/sidebar-icons/%2")
                .arg(AppManager::ins()->resolveThemeName(), sidebarIconFile);
            label->setPixmap(QIcon(path).pixmap(Dpi::scale(36, 36)));
        };
        apply();
        QObject::connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, label, apply);
    }

    // Clears a scroll-area content widget's background without touching its
    // children. A bare "background-color:transparent;" widget stylesheet
    // cascades to every descendant and wipes out card fills and accent bars.
    static void
    makeBackgroundTransparent(QWidget *widget)
    {
        if (widget->objectName().isEmpty())
            widget->setObjectName(QStringLiteral("transparentContent"));
        widget->setStyleSheet(QStringLiteral("#%1{background-color:transparent;}").arg(widget->objectName()));
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
