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
        // Qt6 enables automatic high-DPI scaling by default: logical
        // coordinates stay at 1× and the framework handles the 2× (or
        // higher) physical rendering behind the scenes.  Reading
        // devicePixelRatio() and multiplying again would double every
        // dimension in logical space, making the UI 4× on Retina.
        //
        // Only apply manual scaling when Qt's built-in DPI scaling has
        // been explicitly disabled via the environment variable
        // QT_ENABLE_HIGHDPI_SCALING=0.
        QByteArray env = qgetenv("QT_ENABLE_HIGHDPI_SCALING");
        if (!env.isEmpty() && env == "0") {
            QScreen *screen = QGuiApplication::primaryScreen();
            if (screen) {
                qreal dpr = screen->devicePixelRatio();
                if (dpr > 1.0)
                    return dpr;
            }
        }
        return 1.0;
    }
};

#endif // DPI_H
