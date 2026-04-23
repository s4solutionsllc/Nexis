#ifndef COMMAND_PALETTE_H
#define COMMAND_PALETTE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <functional>

struct CommandItem {
    QString name;
    QString category;
    std::function<void()> action;
};

class CommandPalette : public QWidget
{
    Q_OBJECT

public:
    explicit CommandPalette(QWidget *parent = nullptr);

    void addCommand(const QString &name, const QString &category, std::function<void()> action);
    void show();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void buildLayout();
    void filterCommands(const QString &text);
    void executeSelected();

    void refreshThemeColors();

    QLineEdit *mSearchBox;
    QListWidget *mResultsList;
    QWidget *mFooter = nullptr;
    QList<CommandItem> mCommands;
    QGraphicsDropShadowEffect *mShadowEffect;
};

#endif // COMMAND_PALETTE_H
