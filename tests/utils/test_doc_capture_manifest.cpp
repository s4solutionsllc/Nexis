#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QHash>
#include <QSet>

#include "Common/doc_capture_frame.h"
#include "Common/doc_capture_manifest.h"

class TestDocCaptureManifest : public QObject
{
    Q_OBJECT

private:
    static QByteArray bundledManifest()
    {
        QFile f(QStringLiteral(PROJECT_SOURCE_DIR) + "/shared/nexis/static/doc_capture/manifest.json");
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

private slots:
    void parsesMinimalManifest()
    {
        const QByteArray json = R"({"version":1,"shots":[
            {"file":"a/b.png","page":"dashboard"},
            {"file":"c.png","page":"settings","platforms":["linux"],
             "steps":[{"do":"click","object":"btnX"},{"do":"wait","seconds":2}],
             "after":[{"do":"key","text":"Escape"}]}
        ]})";
        QString error;
        const auto mac = DocCaptureManifest::parse(json, "macos", &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(mac.size(), 1);
        QCOMPARE(mac.at(0).file, QStringLiteral("a/b.png"));
        QVERIFY(mac.at(0).frame);
        QCOMPARE(mac.at(0).settle, 1.5);

        const auto linuxShots = DocCaptureManifest::parse(json, "linux", &error);
        QCOMPARE(linuxShots.size(), 2);
        QCOMPARE(linuxShots.at(1).steps.size(), 2);
        QCOMPARE(linuxShots.at(1).steps.at(0).type, DocCaptureStep::Type::Click);
        QCOMPARE(linuxShots.at(1).steps.at(1).seconds, 2.0);
        QCOMPARE(linuxShots.at(1).after.at(0).type, DocCaptureStep::Type::Key);
    }

    void rejectsInvalidManifests_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("expected");
        QTest::newRow("not json") << QByteArray("{") << "not valid JSON";
        QTest::newRow("wrong version") << QByteArray(R"({"version":2,"shots":[]})") << "version";
        QTest::newRow("no shots") << QByteArray(R"({"version":1})") << "shots";
        QTest::newRow("absolute path") << QByteArray(R"({"version":1,"shots":[{"file":"/etc/x.png"}]})") << "relative";
        QTest::newRow("dotdot path") << QByteArray(R"({"version":1,"shots":[{"file":"../x.png"}]})") << "relative";
        QTest::newRow("not png") << QByteArray(R"({"version":1,"shots":[{"file":"x.jpg"}]})") << "relative";
        QTest::newRow("unknown platform") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","platforms":["windows"]}]})") << "unknown platform";
        QTest::newRow("unknown step") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","steps":[{"do":"rm"}]}]})") << "unknown step";
        QTest::newRow("click without target") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","steps":[{"do":"click"}]}]})") << "click needs";
        QTest::newRow("wait too long") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","steps":[{"do":"wait","seconds":9999}]}]})") << "seconds";
        QTest::newRow("scroll out of range") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","steps":[{"do":"scroll","position":2}]}]})") << "position";
        QTest::newRow("blur not array") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","blur":"lblName"}]})") << "blur";
        QTest::newRow("blur empty name") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","blur":[""]}]})") << "blur";
        QTest::newRow("blurColumns not array") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","blurColumns":"Name"}]})") << "blurColumns";
        QTest::newRow("showColumn without text") << QByteArray(R"({"version":1,"shots":[{"file":"x.png","steps":[{"do":"showColumn"}]}]})") << "needs";
        QTest::newRow("duplicate for platform") << QByteArray(R"({"version":1,"shots":[{"file":"x.png"},{"file":"x.png","platforms":["linux"]}]})") << "listed twice";
    }

    void rejectsInvalidManifests()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, expected);
        QString error;
        const auto shots = DocCaptureManifest::parse(json, "macos", &error);
        QVERIFY(shots.isEmpty());
        QVERIFY2(error.contains(expected, Qt::CaseInsensitive), qPrintable(error));
    }

    void parsesBlurList()
    {
        const QByteArray json = R"({"version":1,"shots":[{"file":"x.png","blur":["lblA","lblB"]}]})";
        QString error;
        const auto shots = DocCaptureManifest::parse(json, "linux", &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(shots.at(0).blur, QStringList({"lblA", "lblB"}));
    }

    // Services and Startup Apps list real service/agent names: every shot of
    // those pages must redact them.
    void bundledManifestBlursServiceAndStartupNames()
    {
        const QByteArray json = bundledManifest();
        for (const char *platform : {"macos", "linux"}) {
            QString error;
            for (const DocCaptureShot &s : DocCaptureManifest::parse(json, QString::fromLatin1(platform), &error)) {
                if (s.page == QLatin1String("services"))
                    QVERIFY2(s.blur.contains("lblServiceName") && s.blur.contains("lblServiceDescription"), qPrintable(s.file));
                if (s.page == QLatin1String("startupApps"))
                    QVERIFY2(s.blur.contains("lblStartupAppName") && s.blur.contains("lblStartupAppPath"), qPrintable(s.file));
            }
        }
    }

    void parsesBlurColumnsAndShowColumn()
    {
        const QByteArray json = R"({"version":1,"shots":[{"file":"x.png","blurColumns":["Name","Process"],
            "steps":[{"do":"showColumn","text":"GPU %"}]}]})";
        QString error;
        const auto shots = DocCaptureManifest::parse(json, "linux", &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(shots.at(0).blurColumns, QStringList({"Name", "Process"}));
        QCOMPARE(shots.at(0).steps.at(0).type, DocCaptureStep::Type::ShowColumn);
        QCOMPARE(shots.at(0).steps.at(0).text, QStringLiteral("GPU %"));
    }

    // Tables that list machine-specific names (process commands and users,
    // boot units, Docker images, hosts entries) must be redacted in every
    // shot of those pages.
    void bundledManifestBlursNameColumns()
    {
        const QHash<QString, QStringList> required = {
            {"processes",    {"User", "Process"}},   // + "Name" on Linux (column is Linux-only)
            {"bootAnalysis", {"Service"}},
            {"docker",       {"Image", "Container", "Volume", "Mount Point"}},
            {"helpers",      {"Full Qualified", "Aliases"}},
        };
        const QByteArray json = bundledManifest();
        for (const char *platform : {"macos", "linux"}) {
            QString error;
            for (const DocCaptureShot &s : DocCaptureManifest::parse(json, QString::fromLatin1(platform), &error)) {
                if (!required.contains(s.page))
                    continue;
                for (const QString &col : required.value(s.page))
                    QVERIFY2(s.blurColumns.contains(col), qPrintable(s.file + " missing " + col));
            }
        }
    }

    void blurRectRemovesDetailOnlyInsideRect()
    {
        // Alternating 1px black/white stripes: maximal detail.
        QImage img(240, 120, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                img.setPixelColor(x, y, (x % 2) ? Qt::white : Qt::black);

        const QRect r(40, 20, 120, 60);
        DocCaptureFrame::blurRect(img, r);

        // Inside: neighbouring pixels are now nearly equal (stripes gone).
        int maxStep = 0;
        for (int x = r.left() + 4; x < r.right() - 4; ++x)
            maxStep = qMax(maxStep, qAbs(img.pixelColor(x, r.center().y()).red()
                                         - img.pixelColor(x + 1, r.center().y()).red()));
        QVERIFY2(maxStep < 40, qPrintable(QString::number(maxStep)));

        // Outside: untouched.
        QCOMPARE(img.pixelColor(10, 10), QColor(Qt::black));
        QCOMPARE(img.pixelColor(11, 10), QColor(Qt::white));
        QCOMPARE(img.pixelColor(r.right() + 5, r.bottom() + 5).red() % 255, 0);
    }

    void blurRectClipsToImage()
    {
        QImage img(50, 50, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::red);
        DocCaptureFrame::blurRect(img, QRect(40, 40, 100, 100));   // must not crash
        DocCaptureFrame::blurRect(img, QRect(200, 200, 10, 10));   // fully outside: no-op
        QCOMPARE(img.pixelColor(0, 0), QColor(Qt::red));
    }

    void samePathAllowedOncePerPlatform()
    {
        const QByteArray json = R"({"version":1,"shots":[
            {"file":"x.png","platforms":["macos"]},{"file":"x.png","platforms":["linux"]}]})";
        QString error;
        QCOMPARE(DocCaptureManifest::parse(json, "linux", &error).size(), 1);
        QVERIFY(error.isEmpty());
    }

    void safeOutputPath()
    {
        QVERIFY(DocCaptureManifest::isSafeOutputPath("website/public/images/guide/x.png"));
        QVERIFY(!DocCaptureManifest::isSafeOutputPath(""));
        QVERIFY(!DocCaptureManifest::isSafeOutputPath("a//b.png"));
        QVERIFY(!DocCaptureManifest::isSafeOutputPath("a/./b.png"));
        QVERIFY(!DocCaptureManifest::isSafeOutputPath("~/x.png"));
        QVERIFY(!DocCaptureManifest::isSafeOutputPath("a\\b.png"));
    }

    void filterMatchesPathOrName()
    {
        QList<DocCaptureShot> shots;
        for (const char *f : {"screenshots/Nexis-01.png", "screenshots/Nexis-02.png", "website/guide/docker.png"}) {
            DocCaptureShot s;
            s.file = QString::fromLatin1(f);
            shots << s;
        }
        QCOMPARE(DocCaptureManifest::filter(shots, "").size(), 3);
        QCOMPARE(DocCaptureManifest::filter(shots, "Nexis-0*").size(), 2);
        QCOMPARE(DocCaptureManifest::filter(shots, "docker.png").size(), 1);
        QCOMPARE(DocCaptureManifest::filter(shots, "screenshots/*, docker.png").size(), 3);
        QCOMPARE(DocCaptureManifest::filter(shots, "nothing").size(), 0);
    }

    // The bundled manifest must parse on both platforms.
    void bundledManifestIsValid()
    {
        const QByteArray json = bundledManifest();
        QVERIFY(!json.isEmpty());
        for (const char *platform : {"macos", "linux"}) {
            QString error;
            const auto shots = DocCaptureManifest::parse(json, QString::fromLatin1(platform), &error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY(!shots.isEmpty());
        }
    }

    // Every screenshot the docs reference must be produced by the manifest on
    // some platform, so a new guide image can't silently go stale.
    void bundledManifestCoversDocImages()
    {
        const QByteArray json = bundledManifest();
        QSet<QString> produced;
        for (const char *platform : {"macos", "linux"}) {
            QString error;
            for (const DocCaptureShot &s : DocCaptureManifest::parse(json, QString::fromLatin1(platform), &error))
                produced.insert(s.file);
        }

        // Captured by hand: a QMenu popup the runner can't open without
        // blocking the event loop.
        const QSet<QString> manual = {
            QStringLiteral("website/public/images/guide/dashboard-availablestyles.png"),
        };

        const QString root = QStringLiteral(PROJECT_SOURCE_DIR);
        QStringList sources = {root + "/README.md",
                               root + "/website/src/components/Screenshots.astro",
                               root + "/linux/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml"};
        QDirIterator it(root + "/website/src/content/guide", {"*.md"}, QDir::Files);
        while (it.hasNext())
            sources << it.next();

        const QRegularExpression re(
            R"((?:/Nexis/|native/)((?:website/public/)?images/(?:guide|screenshots)/[\w.-]+\.png|screenshots/Nexis-\d+\.png))");
        QStringList missing;
        for (const QString &path : sources) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                continue;
            auto m = re.globalMatch(QString::fromUtf8(f.readAll()));
            while (m.hasNext()) {
                QString rel = m.next().captured(1);
                if (rel.startsWith("images/"))
                    rel.prepend("website/public/");
                if (!produced.contains(rel) && !manual.contains(rel) && !missing.contains(rel))
                    missing << rel;
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable("not in manifest: " + missing.join(", ")));
    }

    void frameAddsChromeAndMargins()
    {
        QImage content(200, 100, QImage::Format_ARGB32_Premultiplied);
        content.fill(Qt::red);
        DocCaptureFrame::Style style;
        style.titleBar = Qt::darkGray;
        style.titleText = Qt::lightGray;
        style.border = Qt::gray;
        style.shadow = QColor(0, 0, 0, 100);
        style.closeButton = Qt::red;
        style.minButton = Qt::yellow;
        style.maxButton = Qt::green;

        const QImage out = DocCaptureFrame::decorate(content, "Title", style);
        const int m = DocCaptureFrame::kShadowMargin;
        const int bar = DocCaptureFrame::kTitleBarHeight;
        QCOMPARE(out.width(), 200 + 2 * m);
        QCOMPARE(out.height(), 100 + bar + 2 * m);
        // Content sits under the title bar.
        QCOMPARE(out.pixelColor(m + 100, m + bar + 50), QColor(Qt::red));
        // Corners of the canvas stay transparent.
        QCOMPARE(out.pixelColor(0, 0).alpha(), 0);
    }

    void frameScalesWithDevicePixelRatio()
    {
        QImage content(400, 200, QImage::Format_ARGB32_Premultiplied);
        content.setDevicePixelRatio(2.0);
        content.fill(Qt::blue);
        const QImage out = DocCaptureFrame::decorate(content, QString(), DocCaptureFrame::Style{});
        QCOMPARE(out.width(), 400 + 2 * 2 * DocCaptureFrame::kShadowMargin);
        QCOMPARE(out.height(), 200 + 2 * (DocCaptureFrame::kTitleBarHeight + 2 * DocCaptureFrame::kShadowMargin));
        QCOMPARE(out.devicePixelRatio(), 2.0);
    }
};

QTEST_MAIN(TestDocCaptureManifest)
#include "test_doc_capture_manifest.moc"
