#include <QTest>
#include "Tools/repo_knowledge_base.h"

class TestRepoKnowledgeBase : public QObject
{
    Q_OBJECT

private slots:
    void lookup_ubuntuMain();
    void lookup_ppaDeadsnakes();
    void lookup_dockerCE();
    void lookup_unknownRepo_returnsEmpty();
    void lookup_partialMatch();
    void lookup_caseInsensitive();
    void domainFromUri_validUrl();
    void domainFromUri_noScheme();
    void domainFromUri_emptyInput();
};

void TestRepoKnowledgeBase::lookup_ubuntuMain()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://archive.ubuntu.com/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QCOMPARE(info.name, QString("Ubuntu Main"));
}

void TestRepoKnowledgeBase::lookup_ppaDeadsnakes()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://ppa.launchpadcontent.net/deadsnakes/ppa/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("Deadsnakes"));
}

void TestRepoKnowledgeBase::lookup_dockerCE()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://download.docker.com/linux/ubuntu");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("Docker"));
}

void TestRepoKnowledgeBase::lookup_unknownRepo_returnsEmpty()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://totally-unknown-repo.example.com/apt");
    QVERIFY(info.name.isEmpty());
    QVERIFY(info.description.isEmpty());
}

void TestRepoKnowledgeBase::lookup_partialMatch()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("https://packages.microsoft.com/repos/vscode");
    QVERIFY(!info.name.isEmpty());
    QVERIFY(info.name.contains("VS Code") || info.name.contains("Visual Studio Code"));
}

void TestRepoKnowledgeBase::lookup_caseInsensitive()
{
    RepoKnownInfo info = RepoKnowledgeBase::lookup("http://ARCHIVE.UBUNTU.COM/ubuntu");
    QVERIFY(!info.name.isEmpty());
}

void TestRepoKnowledgeBase::domainFromUri_validUrl()
{
    QString domain = RepoKnowledgeBase::domainFromUri("https://packages.microsoft.com/repos/vscode");
    QCOMPARE(domain, QString("packages.microsoft.com"));
}

void TestRepoKnowledgeBase::domainFromUri_noScheme()
{
    QString domain = RepoKnowledgeBase::domainFromUri("packages.microsoft.com/repos/vscode");
    QVERIFY(domain.isEmpty());
}

void TestRepoKnowledgeBase::domainFromUri_emptyInput()
{
    QString domain = RepoKnowledgeBase::domainFromUri("");
    QVERIFY(domain.isEmpty());
}

QTEST_MAIN(TestRepoKnowledgeBase)
#include "test_repo_knowledge_base.moc"
