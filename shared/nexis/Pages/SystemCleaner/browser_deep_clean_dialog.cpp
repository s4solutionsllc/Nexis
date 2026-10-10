#include "browser_deep_clean_dialog.h"
#include "Common/dialog_buttons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

BrowserDeepCleanDialog::BrowserDeepCleanDialog(const QList<BrowserProfileLocator::Profile> &profiles,
                                               const QStringList &keptCookieDomains,
                                               QWidget *parent)
    : QDialog(parent),
      mProfiles(profiles)
{
    setWindowTitle(tr("Browser Deep Clean"));
    setObjectName("browserDeepCleanDialog");
    setMinimumSize(520, 520);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(DialogButtons::dialogSpacing());
    mainLayout->setContentsMargins(DialogButtons::dialogMargins());

    auto *lblTitle = new QLabel(tr("Browser Deep Clean"), this);
    lblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(lblTitle);

    auto *lblIntro = new QLabel(
        tr("Removes browsing history and cookies from the profiles you choose. Bookmarks, "
           "saved passwords and extensions are not touched. Close the browser first; a "
           "profile that is in use is skipped. You review every item before anything is deleted."),
        this);
    lblIntro->setWordWrap(true);
    mainLayout->addWidget(lblIntro);

    mainLayout->addWidget(new QLabel(tr("Profiles"), this));
    mProfileList = new QListWidget(this);
    mProfileList->setObjectName("browserProfileList");
    mProfileList->setAlternatingRowColors(true);
    for (const BrowserProfileLocator::Profile &profile : std::as_const(mProfiles)) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 — %2").arg(profile.browserName, profile.profileName), mProfileList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setToolTip(profile.profileDir);
    }
    mainLayout->addWidget(mProfileList, 2);

    auto *lblKeep = new QLabel(
        tr("Cookies to keep — sites listed here, and their subdomains, stay signed in."), this);
    lblKeep->setWordWrap(true);
    mainLayout->addWidget(lblKeep);

    mDomainList = new QListWidget(this);
    mDomainList->setObjectName("browserKeepDomainList");
    mDomainList->setAlternatingRowColors(true);
    for (const QString &domain : keptCookieDomains) {
        const QString normalized = normalizeDomain(domain);
        if (!normalized.isEmpty() && mDomainList->findItems(normalized, Qt::MatchExactly).isEmpty())
            mDomainList->addItem(normalized);
    }
    mDomainList->sortItems();
    mainLayout->addWidget(mDomainList, 1);

    auto *domainRow = new QHBoxLayout;
    mEditDomain = new QLineEdit(this);
    mEditDomain->setObjectName("editKeepDomain");
    mEditDomain->setPlaceholderText(tr("example.com"));
    domainRow->addWidget(mEditDomain, 1);
    auto *btnAdd = new QPushButton(tr("Add"), this);
    btnAdd->setObjectName("btnAddKeepDomain");
    btnAdd->setCursor(Qt::PointingHandCursor);
    btnAdd->setAutoDefault(false);
    domainRow->addWidget(btnAdd);
    auto *btnRemove = new QPushButton(tr("Remove"), this);
    btnRemove->setObjectName("btnRemoveKeepDomain");
    btnRemove->setCursor(Qt::PointingHandCursor);
    btnRemove->setAutoDefault(false);
    domainRow->addWidget(btnRemove);
    mainLayout->addLayout(domainRow);

    mLblDomainError = new QLabel(this);
    mLblDomainError->setObjectName("lblKeepDomainError");
    mLblDomainError->setProperty("status", "error");
    mLblDomainError->hide();
    mainLayout->addWidget(mLblDomainError);

    DialogButtons::Row buttons = DialogButtons::build(this, tr("Review…"),
                                                      DialogButtons::Confirm::Primary, tr("Cancel"));
    mBtnReview = buttons.confirm;
    mBtnReview->setObjectName("btnReviewBrowserDeepClean");
    mainLayout->addWidget(buttons.box);

    connect(mBtnReview, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnAdd, &QPushButton::clicked, this, &BrowserDeepCleanDialog::onAddDomain);
    connect(mEditDomain, &QLineEdit::returnPressed, this, &BrowserDeepCleanDialog::onAddDomain);
    connect(btnRemove, &QPushButton::clicked, this, &BrowserDeepCleanDialog::onRemoveDomain);
    connect(mProfileList, &QListWidget::itemChanged, this, &BrowserDeepCleanDialog::updateReviewEnabled);
    updateReviewEnabled();
}

QString BrowserDeepCleanDialog::normalizeDomain(const QString &input)
{
    QString host = input.trimmed().toLower();

    const int scheme = host.indexOf(QLatin1String("://"));
    if (scheme >= 0)
        host = host.mid(scheme + 3);
    host = host.section(QLatin1Char('/'), 0, 0);
    host = host.section(QLatin1Char('@'), -1);
    host = host.section(QLatin1Char(':'), 0, 0);
    while (host.startsWith(QLatin1Char('.')))
        host.remove(0, 1);
    while (host.endsWith(QLatin1Char('.')))
        host.chop(1);

    static const QRegularExpression hostRx(
        QStringLiteral("^(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z0-9-]{2,63}$"));
    if (host == QLatin1String("localhost") || hostRx.match(host).hasMatch())
        return host;
    return QString();
}

void BrowserDeepCleanDialog::onAddDomain()
{
    const QString domain = normalizeDomain(mEditDomain->text());
    if (domain.isEmpty()) {
        mLblDomainError->setText(tr("Enter a site name such as example.com."));
        mLblDomainError->show();
        return;
    }
    mLblDomainError->hide();
    if (mDomainList->findItems(domain, Qt::MatchExactly).isEmpty()) {
        mDomainList->addItem(domain);
        mDomainList->sortItems();
    }
    mEditDomain->clear();
}

void BrowserDeepCleanDialog::onRemoveDomain()
{
    qDeleteAll(mDomainList->selectedItems());
}

void BrowserDeepCleanDialog::updateReviewEnabled()
{
    mBtnReview->setEnabled(!selectedProfiles().isEmpty());
}

QList<BrowserProfileLocator::Profile> BrowserDeepCleanDialog::selectedProfiles() const
{
    QList<BrowserProfileLocator::Profile> selected;
    for (int i = 0; i < mProfileList->count(); ++i) {
        if (mProfileList->item(i)->checkState() == Qt::Checked)
            selected.append(mProfiles.at(i));
    }
    return selected;
}

QStringList BrowserDeepCleanDialog::keptCookieDomains() const
{
    QStringList domains;
    for (int i = 0; i < mDomainList->count(); ++i)
        domains.append(mDomainList->item(i)->text());
    return domains;
}
