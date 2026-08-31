#ifndef MAC_TWEAKS_WIDGET_H
#define MAC_TWEAKS_WIDGET_H

// SSO-23857: searchable macOS "Tweaks" pane — Finder/Dock/Screenshots/
// Animations/Login Window `defaults` toggles. Follows the Helpers-widget
// pattern (see SwappinessWidget/TrimWidget): built entirely in C++, lazily
// loaded via loadIfNeeded(), reads happen off the UI thread. Only ever
// instantiated on macOS (see HelpersPage), but the class itself has no
// platform-specific code so it stays buildable/testable everywhere.

#include <QMap>
#include <QVersionNumber>
#include <QWidget>
#include <functional>

#include "Tools/mac_tweaks_catalog.h"

class QLineEdit;
class QLabel;
class QVBoxLayout;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QToolButton;
class QFrame;

class MacTweaksWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MacTweaksWidget(QWidget *parent = nullptr);

    void loadIfNeeded();
    void refresh();

signals:
    void allStatusesFetched(QMap<QString, MacDefaultsReadResult> statuses);

private slots:
    void onAllStatusesFetched(QMap<QString, MacDefaultsReadResult> statuses);
    void onFilterTextChanged(const QString &text);
    void refreshThemeColors();

private:
    struct RowWidgets {
        MacTweakDef  def;
        bool         supported = true;
        QFrame      *card = nullptr;
        QLabel      *lblCurrent = nullptr;
        QLabel      *lblGated = nullptr;
        QCheckBox   *chk = nullptr;
        QComboBox   *combo = nullptr;
        QSpinBox    *spin = nullptr;
        QLineEdit   *edit = nullptr;
        QToolButton *btnRevert = nullptr;
        QLabel      *lblResult = nullptr;
    };

    void buildUI();
    QFrame *buildRow(const MacTweakDef &tweak);
    void renderRow(RowWidgets &row, const MacDefaultsReadResult &read);
    void setRowBusy(RowWidgets &row, bool busy);
    void onRowWriteFinished(const QString &id, MacDefaultsWriteResult writeRes);
    void runWrite(const QString &id, std::function<MacDefaultsWriteResult()> op);
    void applyFilter(const QString &text);

    QLineEdit *mSearchBox = nullptr;
    QLabel *mLblLoading = nullptr;
    QVBoxLayout *mListLayout = nullptr;
    QVersionNumber mOsVersion;
    QMap<QString, RowWidgets> mRows;
    QMap<QString, QLabel *> mCategoryHeaders;
    QMap<QString, QList<QString>> mCategoryOrder; // category -> ordered tweak ids
    bool mLoaded = false;
};

#endif // MAC_TWEAKS_WIDGET_H
