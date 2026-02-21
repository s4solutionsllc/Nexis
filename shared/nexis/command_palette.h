#ifndef COMMAND_PALETTE_H
#define COMMAND_PALETTE_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
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

    QLineEdit *mSearchBox;
    QListWidget *mResultsList;
    QList<CommandItem> mCommands;
};

#endif // COMMAND_PALETTE_H
