#ifndef DPI_H
#define DPI_H

#include <QApplication>
#include <QScreen>
#include <QtMath>

class Dpi
{
public:
    static qreal factor()
    {
        static qreal f = computeFactor();
        return f;
    }

    static int scale(int px)
    {
        return qRound(px * factor());
    }

    static QSize scale(int w, int h)
    {
        return QSize(qRound(w * factor()), qRound(h * factor()));
    }

    static QSize scale(QSize s)
    {
        return QSize(qRound(s.width() * factor()), qRound(s.height() * factor()));
    }

private:
    static qreal computeFactor()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen)
            return 1.0;
        qreal dpr = screen->devicePixelRatio();
        return (dpr > 1.0) ? dpr : 1.0;
    }
};

#endif // DPI_H
