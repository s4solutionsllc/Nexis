#include "sparkle_embedded_version.h"

#include <Utils/command_util.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QXmlStreamReader>
#include <QDebug>

namespace SparkleEmbeddedVersion {

namespace {

// Parses PackageInfo (component package) or Distribution (product archive)
// XML for the first version="..." attribute — checks <pkg-info> first (the
// package's own declared version), falling back to the first <pkg-ref>
// version if no pkg-info version is present. Never executes anything; this
// is a pure XML scan.
QString firstVersionAttribute(const QByteArray &xml)
{
    QXmlStreamReader xr(xml);
    QString pkgRefVersion;
    while (!xr.atEnd() && !xr.hasError()) {
        xr.readNext();
        if (xr.tokenType() != QXmlStreamReader::StartElement)
            continue;
        if (xr.name() == QLatin1String("pkg-info")) {
            const QString v = xr.attributes().value(QLatin1String("version")).toString();
            if (!v.isEmpty())
                return v;
        } else if (xr.name() == QLatin1String("pkg-ref") && pkgRefVersion.isEmpty()) {
            const QString v = xr.attributes().value(QLatin1String("version")).toString();
            if (!v.isEmpty())
                pkgRefVersion = v;
        }
    }
    return pkgRefVersion;
}

QString readCFBundleShortVersionString(const QString &infoPlistPath)
{
    if (!QFileInfo::exists(infoPlistPath))
        return {};
    const ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("/usr/bin/plutil"),
        {"-convert", "json", "-o", "-", infoPlistPath}, 30000);
    if (!r.ok())
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(r.output.trimmed().toUtf8());
    if (doc.isNull())
        return {};
    return doc.object().value(QLatin1String("CFBundleShortVersionString")).toString();
}

} // namespace

Format formatFromUrl(const QString &enclosureUrl)
{
    const QString path = QUrl(enclosureUrl).path();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("pkg"))
        return Format::Pkg;
    if (suffix == QLatin1String("zip"))
        return Format::Zip;
    return Format::Unsupported;
}

QString extractPkgVersion(const QString &pkgPath, const QString &expandDir)
{
    if (QFileInfo::exists(expandDir)) {
        qWarning() << "sparkle_embedded_version: expandDir already exists, refusing:" << expandDir;
        return {};
    }

    const ExecResult r = CommandUtil::execWithStatus(
        QStringLiteral("/usr/sbin/pkgutil"), {"--expand", pkgPath, expandDir}, 60000);
    if (!r.ok()) {
        qWarning() << "sparkle_embedded_version: pkgutil --expand failed:" << r.error;
        return {};
    }

    // Component package (most Sparkle .pkg installers): PackageInfo at the
    // expand root. Product archive (built with productbuild): Distribution
    // at the expand root instead.
    for (const QString &fname : {QStringLiteral("PackageInfo"), QStringLiteral("Distribution")}) {
        const QString xmlPath = expandDir + QLatin1Char('/') + fname;
        QFile f(xmlPath);
        if (!f.exists() || !f.open(QIODevice::ReadOnly))
            continue;
        const QString version = firstVersionAttribute(f.readAll());
        if (!version.isEmpty())
            return version;
    }
    return {};
}

QString findTopLevelAppBundle(const QString &expandedDir)
{
    QDir dir(expandedDir);
    const QStringList apps = dir.entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (apps.size() != 1)
        return {};
    return dir.absoluteFilePath(apps.first());
}

QString readAppVersionFromExpandedZip(const QString &expandedDir)
{
    const QString appPath = findTopLevelAppBundle(expandedDir);
    if (appPath.isEmpty())
        return {};
    return readCFBundleShortVersionString(appPath + QLatin1String("/Contents/Info.plist"));
}

} // namespace SparkleEmbeddedVersion
