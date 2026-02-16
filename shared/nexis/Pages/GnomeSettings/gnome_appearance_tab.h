#ifndef GNOME_APPEARANCE_TAB_H
#define GNOME_APPEARANCE_TAB_H

#include <QWidget>

class QFontComboBox;
class QSpinBox;

namespace Ui {
    class GnomeAppearanceTab;
}

class GnomeAppearanceTab : public QWidget
{
    Q_OBJECT

public:
    explicit GnomeAppearanceTab(QWidget *parent = nullptr);
    ~GnomeAppearanceTab();

signals:
    void settingFailed(const QString &message);

private:
    void loadSettings();
    QStringList discoverGtkThemes();
    QStringList discoverIconThemes();
    QStringList discoverCursorThemes();
    static void parseFontValue(const QString &value, QString &family, int &size);
    void applyFont(const QString &schema, const QString &key,
                   QFontComboBox *combo, QSpinBox *spin, const QString &label);

    Ui::GnomeAppearanceTab *ui;
    bool mLoading;
};

#endif // GNOME_APPEARANCE_TAB_H
