#ifndef BROWSER_DEEP_CLEAN_DIALOG_H
#define BROWSER_DEEP_CLEAN_DIALOG_H

#include <QDialog>
#include <QStringList>

#include <Utils/browser_profile_locator.h>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// SSO-25782: picks which browser profiles to deep-clean and which cookie
// domains to keep, then hands off to the caller, which runs
// BrowserSqliteCleaner through the Trust & Safety preview dialog. This
// dialog deletes nothing itself.
class BrowserDeepCleanDialog : public QDialog
{
    Q_OBJECT

public:
    BrowserDeepCleanDialog(const QList<BrowserProfileLocator::Profile> &profiles,
                           const QStringList &keptCookieDomains,
                           QWidget *parent = nullptr);

    QList<BrowserProfileLocator::Profile> selectedProfiles() const;
    QStringList keptCookieDomains() const;

    // Reduces what a user might type or paste ("https://www.Example.com/path",
    // ".example.com", "example.com:8080") to a bare lower-case host. Returns
    // an empty string when no plausible host is left.
    static QString normalizeDomain(const QString &input);

private slots:
    void onAddDomain();
    void onRemoveDomain();
    void updateReviewEnabled();

private:
    QList<BrowserProfileLocator::Profile> mProfiles;
    QListWidget *mProfileList;
    QListWidget *mDomainList;
    QLineEdit *mEditDomain;
    QLabel *mLblDomainError;
    QPushButton *mBtnReview;
};

#endif // BROWSER_DEEP_CLEAN_DIALOG_H
