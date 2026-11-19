#ifndef LINK_REPLACE_DIALOG_H
#define LINK_REPLACE_DIALOG_H

#include <QDialog>
#include <QList>
#include <QStringList>

#include "Services/duplicate_finder_service.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QTreeWidget;

// SSO-27146: the preview step of "replace duplicates with links". Lists, for
// every ticked duplicate, which file it would become a link to and — for the
// ones that can't be linked — why not. Nothing on disk changes until the user
// confirms; the page then hands plan() to DuplicateFinderService.
class LinkReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    LinkReplaceDialog(DuplicateFinderService *service, const QStringList &selected,
                      const QList<DuplicateGroup> &groups, QWidget *parent = nullptr);

    QList<LinkReplaceEntry> plan() const { return mPlan; }

    static QString statusText(LinkReplaceEntry::Status status);

private:
    void rebuildPlan();

    DuplicateFinderService *mService;
    QStringList mSelected;
    QList<DuplicateGroup> mGroups;
    QList<LinkReplaceEntry> mPlan;

    QRadioButton *mRadioHard = nullptr;
    QRadioButton *mRadioSymbolic = nullptr;
    QTreeWidget *mTree = nullptr;
    QLabel *mLblSummary = nullptr;
    QPushButton *mBtnReplace = nullptr;
};

#endif // LINK_REPLACE_DIALOG_H
