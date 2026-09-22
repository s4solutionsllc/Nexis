// SSO-25047 (GH#475 follow-up): BtmRow::setAvailableWidth() elides the
// name/identifier/path labels so the row's sizeHint never exceeds the width
// the page hands it. Regression coverage for the underlying bug:
// StartupAppsPage captures row->sizeHint() once and passes it straight to
// QListWidgetItem::setSizeHint() with the list's horizontal scrollbar off,
// so an unbounded label sizeHint baked an oversized fixed item width into
// the list, pushing the status badges (and part of the row) past the
// viewport with no way to reach them. No interactive desktop is available
// in CI, so this exercises the real widget headlessly instead of requiring
// a manual narrow-width click-through — see DiskMapDeleteConfirmTests for
// the same rationale applied to a different widget.

#include <QtTest>
#include <QApplication>
#include <QLabel>

#include "Pages/StartupApps/btm_row.h"

namespace {
BtmRecord makeRecord(const QString &identifier, const QString &executablePath)
{
    BtmRecord r;
    r.identifier = identifier;
    r.executablePath = executablePath;
    r.type = BtmRecordType::LoginItem;
    r.enabled = true;
    return r;
}
} // namespace

class TestBtmRow : public QObject
{
    Q_OBJECT
private slots:
    void longIdentifierAndPath_rowNeverWiderThanAvailableWidth();
    void pathologicallyNarrowWidth_stillProducesUsableRow();
    void elidedLabels_keepFullTextAsTooltip();
};

void TestBtmRow::longIdentifierAndPath_rowNeverWiderThanAvailableWidth()
{
    const QString longIdentifier = QStringLiteral(
        "com.example.vendor.some.very.long.reverse.dns.bundle.identifier.helper");
    const QString longPath = QStringLiteral(
        "/Users/exampleuser/Library/Application Support/SomeVendor/SomeApp/"
        "Contents/Resources/HelperTool.app/Contents/MacOS/HelperTool");

    BtmRow row(makeRecord(longIdentifier, longPath));

    // Before setAvailableWidth() is called this still reflects the full,
    // unbounded text — that captured-at-construction value is exactly what
    // StartupAppsPage used to bake into QListWidgetItem::setSizeHint().
    QVERIFY(row.sizeHint().width() > 320);

    row.setAvailableWidth(320);
    QVERIFY(row.sizeHint().width() <= 320);
}

void TestBtmRow::pathologicallyNarrowWidth_stillProducesUsableRow()
{
    BtmRow row(makeRecord(QStringLiteral("com.example.app"), QStringLiteral("/usr/bin/app")));

    // Narrower than badges + margins alone can fit into — the floor in
    // setAvailableWidth() must keep the row from collapsing to zero/negative
    // width instead of propagating a nonsensical size to the list item.
    row.setAvailableWidth(10);
    QVERIFY(row.sizeHint().width() > 0);
}

void TestBtmRow::elidedLabels_keepFullTextAsTooltip()
{
    const QString longIdentifier = QStringLiteral(
        "com.example.vendor.some.very.long.reverse.dns.bundle.identifier.helper");
    const QString longPath = QStringLiteral("/usr/local/some/very/long/path/to/an/executable-binary");

    BtmRow row(makeRecord(longIdentifier, longPath));
    row.setAvailableWidth(280);

    const auto labels = row.findChildren<QLabel *>(QStringLiteral("lblBtmRowSubtext"));
    QCOMPARE(labels.size(), 1);
    QLabel *lblSub = labels.first();

    // The displayed text is now shorter than the full identifier/path pair,
    // but the tooltip still carries the untruncated value.
    QVERIFY(lblSub->text().length() < (longIdentifier.length() + longPath.length()));
    QVERIFY(lblSub->toolTip().contains(longIdentifier));
    QVERIFY(lblSub->toolTip().contains(longPath));
}

QTEST_MAIN(TestBtmRow)
#include "test_btm_row.moc"
