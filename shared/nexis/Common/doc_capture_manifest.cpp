#include "doc_capture_manifest.h"

#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace {

const QHash<QString, DocCaptureStep::Type> &stepTypes()
{
    static const QHash<QString, DocCaptureStep::Type> kTypes = {
        {"click",       DocCaptureStep::Type::Click},
        {"setText",     DocCaptureStep::Type::SetText},
        {"wait",        DocCaptureStep::Type::Wait},
        {"waitEnabled", DocCaptureStep::Type::WaitEnabled},
        {"waitUptime",  DocCaptureStep::Type::WaitUptime},
        {"scroll",      DocCaptureStep::Type::Scroll},
        {"expandTrees", DocCaptureStep::Type::ExpandTrees},
        {"sort",        DocCaptureStep::Type::Sort},
        {"showPalette", DocCaptureStep::Type::ShowPalette},
        {"key",         DocCaptureStep::Type::Key},
        {"closeDialog", DocCaptureStep::Type::CloseDialog},
        {"showColumn",  DocCaptureStep::Type::ShowColumn},
    };
    return kTypes;
}

bool parseStep(const QJsonValue &value, DocCaptureStep *out, QString *why)
{
    if (!value.isObject()) {
        *why = QStringLiteral("step is not an object");
        return false;
    }
    const QJsonObject o = value.toObject();
    const QString kind = o.value("do").toString();
    const auto it = stepTypes().constFind(kind);
    if (it == stepTypes().constEnd()) {
        *why = QStringLiteral("unknown step \"%1\"").arg(kind);
        return false;
    }

    DocCaptureStep s;
    s.type = it.value();
    s.object = o.value("object").toString();
    s.text = o.value("text").toString();
    s.seconds = o.value("seconds").toDouble(0);
    s.position = o.value("position").toDouble(1.0);
    s.descending = o.value("order").toString("desc") != QLatin1String("asc");

    switch (s.type) {
    case DocCaptureStep::Type::Click:
        if (s.object.isEmpty() && s.text.isEmpty()) {
            *why = QStringLiteral("click needs \"object\" or \"text\"");
            return false;
        }
        break;
    case DocCaptureStep::Type::SetText:
        if (s.object.isEmpty() || !o.contains("text")) {
            *why = QStringLiteral("setText needs \"object\" and \"text\"");
            return false;
        }
        break;
    case DocCaptureStep::Type::Wait:
    case DocCaptureStep::Type::WaitUptime:
        if (s.seconds <= 0 || s.seconds > 600) {
            *why = QStringLiteral("%1 needs 0 < \"seconds\" <= 600").arg(kind);
            return false;
        }
        break;
    case DocCaptureStep::Type::WaitEnabled:
        if (s.object.isEmpty()) {
            *why = QStringLiteral("waitEnabled needs \"object\"");
            return false;
        }
        if (s.seconds <= 0)
            s.seconds = 60;
        if (s.seconds > 600) {
            *why = QStringLiteral("waitEnabled \"seconds\" must be <= 600");
            return false;
        }
        break;
    case DocCaptureStep::Type::Scroll:
        if (s.position < 0 || s.position > 1) {
            *why = QStringLiteral("scroll \"position\" must be between 0 and 1");
            return false;
        }
        break;
    case DocCaptureStep::Type::Sort:
    case DocCaptureStep::Type::Key:
    case DocCaptureStep::Type::CloseDialog:
    case DocCaptureStep::Type::ShowColumn:
        if (s.text.isEmpty()) {
            *why = QStringLiteral("%1 needs \"text\"").arg(kind);
            return false;
        }
        break;
    case DocCaptureStep::Type::ExpandTrees:
    case DocCaptureStep::Type::ShowPalette:
        break;
    }

    *out = s;
    return true;
}

bool parseSteps(const QJsonValue &value, QList<DocCaptureStep> *out, QString *why)
{
    if (value.isUndefined() || value.isNull())
        return true;
    if (!value.isArray()) {
        *why = QStringLiteral("steps must be an array");
        return false;
    }
    const QJsonArray arr = value.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        DocCaptureStep step;
        QString stepWhy;
        if (!parseStep(arr.at(i), &step, &stepWhy)) {
            *why = QStringLiteral("step %1: %2").arg(i + 1).arg(stepWhy);
            return false;
        }
        out->append(step);
    }
    return true;
}

} // namespace

namespace DocCaptureManifest {

QString currentPlatform()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

bool isSafeOutputPath(const QString &file)
{
    if (file.isEmpty() || file.contains(QLatin1Char('\\')))
        return false;
    if (QFileInfo(file).isAbsolute() || file.startsWith(QLatin1Char('/')) || file.startsWith(QLatin1Char('~')))
        return false;
    if (!file.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        return false;
    const QStringList parts = file.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QLatin1String("..") || part == QLatin1String("."))
            return false;
    }
    return true;
}

QList<DocCaptureShot> parse(const QByteArray &json, const QString &platform, QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error)
            *error = msg;
        return QList<DocCaptureShot>();
    };

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError)
        return fail(QStringLiteral("manifest is not valid JSON: %1").arg(perr.errorString()));
    if (!doc.isObject())
        return fail(QStringLiteral("manifest must be a JSON object"));

    const QJsonObject root = doc.object();
    if (root.value("version").toInt() != 1)
        return fail(QStringLiteral("unsupported manifest version (expected 1)"));
    if (!root.value("shots").isArray())
        return fail(QStringLiteral("manifest has no \"shots\" array"));

    static const QStringList kPlatforms = {QStringLiteral("macos"), QStringLiteral("linux")};

    QList<DocCaptureShot> result;
    QHash<QString, QString> seen;   // "<platform>|<file>" -> already defined
    const QJsonArray shots = root.value("shots").toArray();
    for (int i = 0; i < shots.size(); ++i) {
        if (!shots.at(i).isObject())
            return fail(QStringLiteral("shot %1 is not an object").arg(i + 1));
        const QJsonObject o = shots.at(i).toObject();

        DocCaptureShot shot;
        shot.file = o.value("file").toString();
        const QString label = shot.file.isEmpty() ? QStringLiteral("#%1").arg(i + 1) : shot.file;
        if (!isSafeOutputPath(shot.file))
            return fail(QStringLiteral("shot %1: \"file\" must be a relative .png path without \"..\"").arg(label));

        shot.page = o.value("page").toString();
        shot.dialog = o.value("dialog").toString();
        shot.frame = o.value("frame").toBool(true);

        const QJsonValue blur = o.value("blur");
        if (!blur.isUndefined()) {
            if (!blur.isArray())
                return fail(QStringLiteral("shot %1: \"blur\" must be an array of object names").arg(label));
            for (const QJsonValue &b : blur.toArray()) {
                const QString name = b.toString();
                if (name.isEmpty())
                    return fail(QStringLiteral("shot %1: \"blur\" entries must be non-empty object names").arg(label));
                shot.blur << name;
            }
        }

        const QJsonValue blurCols = o.value("blurColumns");
        if (!blurCols.isUndefined()) {
            if (!blurCols.isArray())
                return fail(QStringLiteral("shot %1: \"blurColumns\" must be an array of column headers").arg(label));
            for (const QJsonValue &b : blurCols.toArray()) {
                const QString header = b.toString();
                if (header.isEmpty())
                    return fail(QStringLiteral("shot %1: \"blurColumns\" entries must be non-empty headers").arg(label));
                shot.blurColumns << header;
            }
        }
        shot.settle = o.value("settle").toDouble(1.5);
        if (shot.settle < 0 || shot.settle > 60)
            return fail(QStringLiteral("shot %1: \"settle\" must be between 0 and 60").arg(label));

        const QJsonValue plats = o.value("platforms");
        if (!plats.isUndefined()) {
            if (!plats.isArray())
                return fail(QStringLiteral("shot %1: \"platforms\" must be an array").arg(label));
            for (const QJsonValue &p : plats.toArray()) {
                const QString name = p.toString();
                if (!kPlatforms.contains(name))
                    return fail(QStringLiteral("shot %1: unknown platform \"%2\"").arg(label, name));
                shot.platforms << name;
            }
        }

        QString why;
        if (!parseSteps(o.value("steps"), &shot.steps, &why))
            return fail(QStringLiteral("shot %1: %2").arg(label, why));
        if (!parseSteps(o.value("after"), &shot.after, &why))
            return fail(QStringLiteral("shot %1: after %2").arg(label, why));

        // The same output file may be listed once per platform, never twice
        // for one platform — the second capture would silently overwrite.
        const QStringList owners = shot.platforms.isEmpty() ? kPlatforms : shot.platforms;
        for (const QString &p : owners) {
            const QString key = p + QLatin1Char('|') + shot.file;
            if (seen.contains(key))
                return fail(QStringLiteral("shot %1 is listed twice for %2").arg(label, p));
            seen.insert(key, shot.file);
        }

        if (shot.platforms.isEmpty() || shot.platforms.contains(platform))
            result.append(shot);
    }
    return result;
}

QList<DocCaptureShot> filter(const QList<DocCaptureShot> &shots, const QString &patterns)
{
    const QStringList pats = patterns.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (pats.isEmpty())
        return shots;

    QList<QRegularExpression> res;
    for (const QString &p : pats)
        res.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(
            p.trimmed(), QRegularExpression::UnanchoredWildcardConversion)));

    QList<DocCaptureShot> out;
    for (const DocCaptureShot &s : shots) {
        const QString name = QFileInfo(s.file).fileName();
        for (const QRegularExpression &re : res) {
            const QRegularExpression anchored(QRegularExpression::anchoredPattern(re.pattern()));
            if (anchored.match(s.file).hasMatch() || anchored.match(name).hasMatch()) {
                out.append(s);
                break;
            }
        }
    }
    return out;
}

} // namespace DocCaptureManifest
