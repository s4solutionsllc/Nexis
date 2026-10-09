#include <QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

#include "browser_deep_clean_dialog.h"

// SSO-25782: the profile + cookie keep-list picker in front of the browser
// deep-clean preview. It deletes nothing itself, so these tests cover only
// what it hands to the caller.

using Profile = BrowserProfileLocator::Profile;

class TestBrowserDeepCleanDialog : public QObject
{
    Q_OBJECT

private:
    static QList<Profile> twoProfiles()
    {
        Profile firefox;
        firefox.family = BrowserProfileLocator::Family::Firefox;
        firefox.browserName = "Firefox";
        firefox.profileName = "default-release";
        firefox.profileDir = "/home/u/.mozilla/firefox/abc.default-release";

        Profile chrome;
        chrome.family = BrowserProfileLocator::Family::Chromium;
        chrome.browserName = "Google Chrome";
        chrome.profileName = "Default";
        chrome.profileDir = "/home/u/.config/google-chrome/Default";
        return {firefox, chrome};
    }

private slots:
    void normalizeDomain_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("plain") << "example.com" << "example.com";
        QTest::newRow("mixed case + spaces") << "  Example.COM " << "example.com";
        QTest::newRow("leading dot") << ".example.com" << "example.com";
        QTest::newRow("url with path") << "https://mail.example.com/inbox?x=1" << "mail.example.com";
        QTest::newRow("port") << "example.com:8443" << "example.com";
        QTest::newRow("userinfo") << "https://user@example.com/" << "example.com";
        QTest::newRow("localhost") << "localhost" << "localhost";
        QTest::newRow("empty") << "" << "";
        QTest::newRow("single label") << "example" << "";
        QTest::newRow("spaces inside") << "exa mple.com" << "";
        QTest::newRow("wildcard") << "*.example.com" << "";
        QTest::newRow("sql-ish") << "a.com' OR '1'='1" << "";
    }

    void normalizeDomain()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(BrowserDeepCleanDialog::normalizeDomain(input), expected);
    }

    void nothingPreselected_reviewDisabledUntilAProfileIsChecked()
    {
        BrowserDeepCleanDialog dialog(twoProfiles(), {});
        auto *list = dialog.findChild<QListWidget *>("browserProfileList");
        auto *review = dialog.findChild<QPushButton *>("btnReviewBrowserDeepClean");
        QVERIFY(list && review);

        QCOMPARE(list->count(), 2);
        QVERIFY(dialog.selectedProfiles().isEmpty());
        QVERIFY(!review->isEnabled());

        list->item(1)->setCheckState(Qt::Checked);
        QVERIFY(review->isEnabled());
        QCOMPARE(dialog.selectedProfiles().size(), 1);
        QCOMPARE(dialog.selectedProfiles().first().browserName, QStringLiteral("Google Chrome"));

        list->item(1)->setCheckState(Qt::Unchecked);
        QVERIFY(!review->isEnabled());
    }

    void keepList_loadsNormalisedAndDeduplicated()
    {
        BrowserDeepCleanDialog dialog(twoProfiles(), {"Example.com", ".example.com", "not a host", "b.org"});
        QCOMPARE(dialog.keptCookieDomains(), (QStringList{"b.org", "example.com"}));
    }

    void keepList_addValidatesAndDeduplicates_removeDeletesSelection()
    {
        BrowserDeepCleanDialog dialog(twoProfiles(), {});
        auto *edit = dialog.findChild<QLineEdit *>("editKeepDomain");
        auto *add = dialog.findChild<QPushButton *>("btnAddKeepDomain");
        auto *remove = dialog.findChild<QPushButton *>("btnRemoveKeepDomain");
        auto *error = dialog.findChild<QLabel *>("lblKeepDomainError");
        auto *domains = dialog.findChild<QListWidget *>("browserKeepDomainList");
        QVERIFY(edit && add && remove && error && domains);

        edit->setText("nonsense");
        add->click();
        QVERIFY(dialog.keptCookieDomains().isEmpty());
        QVERIFY(!error->isHidden());

        edit->setText("https://Example.com/login");
        add->click();
        QVERIFY(error->isHidden());
        QVERIFY(edit->text().isEmpty());
        QCOMPARE(dialog.keptCookieDomains(), QStringList{"example.com"});

        edit->setText("example.com");
        add->click();
        QCOMPARE(dialog.keptCookieDomains(), QStringList{"example.com"});

        domains->item(0)->setSelected(true);
        remove->click();
        QVERIFY(dialog.keptCookieDomains().isEmpty());
    }
};

QTEST_MAIN(TestBrowserDeepCleanDialog)
#include "test_browser_deep_clean_dialog.moc"
