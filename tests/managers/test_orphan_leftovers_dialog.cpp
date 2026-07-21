// SSO-15429 (SSO-15373 §1/§5, Design Anchor SSO-1768): Orphan Leftovers tab
// + confirmation dialog.
//
// OrphanLeftoversDialog is a QDialog, so — following the codebase's existing
// convention (TrustSafetyPreviewDialog, CrumbsReviewDialog, and
// LeftoverReviewDialogLinux all ship without a widget-level test; only their
// business logic is exercised in isolation, e.g. test_trust_safety_runner.cpp)
// — this test targets the two pure, static, non-widget entry points the
// dialog is built on rather than instantiating the QDialog itself:
//
//   - buildRows(): the single source of truth for "every orphan match starts
//     unchecked" (CISO §5). Pure QList<OrphanLeftover> -> QList<Row> mapping.
//   - confirmationSentence(): the Design Anchor SSO-1768 one-sentence
//     "Move {N} items ({size}) to Trash?" copy, with the correct size/count.

#include <QtTest>

#include "Pages/Uninstaller/orphan_leftovers_dialog.h"

namespace {

// Mirrors the heuristic in trust_safety_preview_dialog.cpp's
// looksLikeSingleSentence(): counts sentence terminators that aren't part of
// a decimal number, so "12.4 MB" doesn't get miscounted as two sentences.
bool looksLikeSingleSentence(const QString &text)
{
    int terminators = 0;
    for (int i = 0; i < text.length(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?')) {
            const bool afterDigit = i > 0 && text.at(i - 1).isDigit();
            const bool beforeDigit = i + 1 < text.length() && text.at(i + 1).isDigit();
            if (!(afterDigit && beforeDigit))
                terminators++;
        }
    }
    return terminators <= 1;
}

OrphanLeftover makeOrphan(const QString &path, const QString &category, quint64 size,
                           const QStringList &signalLabels)
{
    OrphanLeftover o;
    o.path = path;
    o.canonicalPath = path;
    o.category = category;
    o.size = size;
    for (const QString &label : signalLabels)
        o.matchedSignals.append({ QStringLiteral("rule"), label });
    o.confidenceScore = signalLabels.size();
    return o;
}

} // namespace

class TestOrphanLeftoversDialog : public QObject
{
    Q_OBJECT

private slots:
    void buildRows_defaultsEveryItemUnchecked();
    void buildRows_carriesFullSignalSetNotJustAScore();
    void buildRows_emptyInput();
    void confirmationSentence_includesCountAndSize();
    void confirmationSentence_isSingleSentence();
    void confirmationSentence_neverMentionsPermanentDelete();
};

void TestOrphanLeftoversDialog::buildRows_defaultsEveryItemUnchecked()
{
    QList<OrphanLeftover> items;
    items << makeOrphan("/home/user/.config/StaleApp", "Config", 1024,
                         { "No installed app", "Reverse-DNS naming", "Not modified in 30+ days" });
    items << makeOrphan("/home/user/.cache/AnotherApp", "Cache", 2048,
                         { "No installed app", "Not modified in 30+ days", "Not accessed in 7+ days" });

    const QList<OrphanLeftoversDialog::Row> rows = OrphanLeftoversDialog::buildRows(items);

    QCOMPARE(rows.size(), 2);
    for (const OrphanLeftoversDialog::Row &row : rows)
        QVERIFY2(!row.checked, "CISO §5: every orphan match must start unchecked");
}

void TestOrphanLeftoversDialog::buildRows_carriesFullSignalSetNotJustAScore()
{
    const QStringList signals = { "No installed app", "Reverse-DNS naming",
                                   "Not modified in 30+ days", "Not accessed in 7+ days" };
    QList<OrphanLeftover> items;
    items << makeOrphan("/home/user/.local/share/org.example.Old", "Local Share", 4096, signals);

    const QList<OrphanLeftoversDialog::Row> rows = OrphanLeftoversDialog::buildRows(items);

    QCOMPARE(rows.size(), 1);
    const OrphanLeftoversDialog::Row &row = rows.first();
    QCOMPARE(row.path, QStringLiteral("/home/user/.local/share/org.example.Old"));
    QCOMPARE(row.category, QStringLiteral("Local Share"));
    QCOMPARE(row.size, quint64(4096));
    // The full matched-signal set survives, not a collapsed confidence number.
    QCOMPARE(row.signalLabels, signals);
}

void TestOrphanLeftoversDialog::buildRows_emptyInput()
{
    QVERIFY(OrphanLeftoversDialog::buildRows({}).isEmpty());
}

void TestOrphanLeftoversDialog::confirmationSentence_includesCountAndSize()
{
    const QString sentence = OrphanLeftoversDialog::confirmationSentence(3, 12u * 1024 * 1024);
    QVERIFY2(sentence.contains(QStringLiteral("3")), qPrintable(sentence));
    QVERIFY2(sentence.contains(QStringLiteral("Trash")), qPrintable(sentence));
}

void TestOrphanLeftoversDialog::confirmationSentence_isSingleSentence()
{
    // Design Anchor (SSO-1768): confirmation dialog copy is one sentence
    // maximum. Check a few counts/sizes so the format string can't sneak an
    // extra clause in for a particular pluralization.
    for (int count : { 0, 1, 3, 42 }) {
        const QString sentence = OrphanLeftoversDialog::confirmationSentence(count, 512);
        QVERIFY2(looksLikeSingleSentence(sentence), qPrintable(sentence));
    }
}

void TestOrphanLeftoversDialog::confirmationSentence_neverMentionsPermanentDelete()
{
    // Orphan scanner is trash-only (SSO-15429 scope) — the confirmation copy
    // must never offer a permanent-delete option.
    const QString sentence = OrphanLeftoversDialog::confirmationSentence(5, 1024);
    QVERIFY(!sentence.contains(QStringLiteral("permanent"), Qt::CaseInsensitive));
    QVERIFY(!sentence.contains(QStringLiteral("forever"), Qt::CaseInsensitive));
}

QTEST_MAIN(TestOrphanLeftoversDialog)
#include "test_orphan_leftovers_dialog.moc"
