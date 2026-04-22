#include "psi_info.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>

void PsiInfo::updateCpuPsi()
{
    mCpu = parseFile("/proc/pressure/cpu");
}

PsiSnapshot PsiInfo::parseFile(const char *path)
{
    PsiSnapshot snap;

    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return snap;

    static const QRegularExpression re(
        QStringLiteral(R"((some|full) avg10=([\d.]+) avg60=([\d.]+) avg300=([\d.]+))"));

    while (!f.atEnd()) {
        const QString line = QString::fromLatin1(f.readLine());
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch())
            continue;

        const bool isSome = (m.captured(1) == QLatin1String("some"));
        const double a10  = m.captured(2).toDouble();
        const double a60  = m.captured(3).toDouble();
        const double a300 = m.captured(4).toDouble();

        if (isSome) {
            snap.someAvg10  = a10;
            snap.someAvg60  = a60;
            snap.someAvg300 = a300;
        } else {
            snap.fullAvg10  = a10;
            snap.fullAvg60  = a60;
            snap.fullAvg300 = a300;
        }
        snap.available = true;
    }

    return snap;
}
