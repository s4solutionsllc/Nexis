#include <QtTest>
#include <Tools/docker_tool.h>

class TestDockerTool : public QObject
{
    Q_OBJECT

private slots:

    // --- parseSizeToBytes ---

    void parseSizeToBytes_bytes()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("512B"), 512);
    }

    void parseSizeToBytes_kilobytes()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("1.5KB"), 1500);
    }

    void parseSizeToBytes_megabytes()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("256MB"), 256000000);
    }

    void parseSizeToBytes_gigabytes()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("1.2GB"), 1200000000);
    }

    void parseSizeToBytes_terabytes()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("2TB"), qint64(2000000000000));
    }

    void parseSizeToBytes_lowercaseKb()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("100kB"), 100000);
    }

    void parseSizeToBytes_zero()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("0B"), 0);
    }

    void parseSizeToBytes_empty()
    {
        QCOMPARE(DockerTool::parseSizeToBytes(""), 0);
    }

    void parseSizeToBytes_malformed()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("foobar"), 0);
        QCOMPARE(DockerTool::parseSizeToBytes("100"), 0);
        QCOMPARE(DockerTool::parseSizeToBytes("MB"), 0);
    }

    void parseSizeToBytes_whitespace()
    {
        QCOMPARE(DockerTool::parseSizeToBytes("  256MB  "), 256000000);
    }

    // --- parseImageLine ---

    void parseImageLine_standard()
    {
        QString line = "abc123|myapp|latest|256MB|2026-03-01 10:00:00";
        QStringList usedRefs = {"myapp:latest"};
        DockerImage img = DockerTool::parseImageLine(line, usedRefs);

        QCOMPARE(img.id, "abc123");
        QCOMPARE(img.repository, "myapp");
        QCOMPARE(img.tag, "latest");
        QCOMPARE(img.size, "256MB");
        QCOMPARE(img.sizeBytes, 256000000);
        QCOMPARE(img.createdAt, "2026-03-01 10:00:00");
        QVERIFY(!img.isDangling);
        QVERIFY(img.isUsed);
    }

    void parseImageLine_dangling()
    {
        QString line = "def456|<none>|<none>|128MB|2026-02-15 08:30:00";
        DockerImage img = DockerTool::parseImageLine(line);

        QCOMPARE(img.id, "def456");
        QVERIFY(img.isDangling);
        QVERIFY(!img.isUsed);
    }

    void parseImageLine_usedById()
    {
        QString line = "abc123|myapp|v2|100MB|2026-03-01 10:00:00";
        QStringList usedRefs = {"abc123"};
        DockerImage img = DockerTool::parseImageLine(line, usedRefs);

        QVERIFY(img.isUsed);
    }

    void parseImageLine_usedByRepo()
    {
        QString line = "abc123|myapp|v2|100MB|2026-03-01 10:00:00";
        QStringList usedRefs = {"myapp"};
        DockerImage img = DockerTool::parseImageLine(line, usedRefs);

        QVERIFY(img.isUsed);
    }

    void parseImageLine_shortLine()
    {
        DockerImage img = DockerTool::parseImageLine("abc|def|ghi");
        QVERIFY(img.id.isEmpty());
    }

    void parseImageLine_empty()
    {
        DockerImage img = DockerTool::parseImageLine("");
        QVERIFY(img.id.isEmpty());
    }

    // --- parseContainerLine ---

    void parseContainerLine_running()
    {
        QString line = "ctr001|web-server|Up 3 hours|running|nginx:latest|0.0.0.0:80->80/tcp|2026-03-20 09:00:00";
        DockerContainer ctr = DockerTool::parseContainerLine(line);

        QCOMPARE(ctr.id, "ctr001");
        QCOMPARE(ctr.name, "web-server");
        QCOMPARE(ctr.status, "Up 3 hours");
        QCOMPARE(ctr.state, "running");
        QCOMPARE(ctr.image, "nginx:latest");
        QCOMPARE(ctr.ports, "0.0.0.0:80->80/tcp");
        QCOMPARE(ctr.createdAt, "2026-03-20 09:00:00");
    }

    void parseContainerLine_exited()
    {
        QString line = "ctr002|old-app|Exited (0) 2 days ago|exited|myapp:v1||2026-03-18 12:00:00";
        DockerContainer ctr = DockerTool::parseContainerLine(line);

        QCOMPARE(ctr.state, "exited");
        QCOMPARE(ctr.ports, "");
    }

    void parseContainerLine_paused()
    {
        QString line = "ctr003|paused-svc|Up 1 hour (Paused)|Paused|redis:7||2026-03-22 14:00:00";
        DockerContainer ctr = DockerTool::parseContainerLine(line);

        QCOMPARE(ctr.state, "paused");
    }

    void parseContainerLine_shortLine()
    {
        DockerContainer ctr = DockerTool::parseContainerLine("id|name|status");
        QVERIFY(ctr.id.isEmpty());
    }

    // --- parseVolumeLine ---

    void parseVolumeLine_standard()
    {
        QString line = "my-volume|local|/var/lib/docker/volumes/my-volume/_data";
        DockerVolume vol = DockerTool::parseVolumeLine(line);

        QCOMPARE(vol.name, "my-volume");
        QCOMPARE(vol.driver, "local");
        QCOMPARE(vol.mountpoint, "/var/lib/docker/volumes/my-volume/_data");
        QVERIFY(vol.isUsed);
    }

    void parseVolumeLine_dangling()
    {
        QString line = "orphan-vol|local|/var/lib/docker/volumes/orphan-vol/_data";
        QStringList danglingNames = {"orphan-vol"};
        DockerVolume vol = DockerTool::parseVolumeLine(line, danglingNames);

        QVERIFY(!vol.isUsed);
    }

    void parseVolumeLine_shortLine()
    {
        DockerVolume vol = DockerTool::parseVolumeLine("name|driver");
        QVERIFY(vol.name.isEmpty());
    }
};

QTEST_MAIN(TestDockerTool)
#include "test_docker_tool.moc"
