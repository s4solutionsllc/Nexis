#include "command_palette.h"
#include <QGraphicsDropShadowEffect>
#include <QApplication>

CommandPalette::CommandPalette(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName("commandPalette");
    setFixedWidth(480);

    buildLayout();
}

void CommandPalette::buildLayout()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(0);

    auto *container = new QWidget(this);
    container->setObjectName("commandPaletteInner");
    auto *innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(0);

    mSearchBox = new QLineEdit(container);
    mSearchBox->setObjectName("commandPaletteSearch");
    mSearchBox->setPlaceholderText(tr("Type a command..."));
    mSearchBox->installEventFilter(this);

    mResultsList = new QListWidget(container);
    mResultsList->setObjectName("commandPaletteResults");
    mResultsList->setFocusPolicy(Qt::NoFocus);
    mResultsList->setMaximumHeight(320);

    innerLayout->addWidget(mSearchBox);
    innerLayout->addWidget(mResultsList);
    mainLayout->addWidget(container);

    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 100));
    container->setGraphicsEffect(shadow);

    connect(mSearchBox, &QLineEdit::textChanged, this, &CommandPalette::filterCommands);
    connect(mResultsList, &QListWidget::itemActivated, this, [this]() {
        executeSelected();
    });
}

void CommandPalette::addCommand(const QString &name, const QString &category, std::function<void()> action)
{
    mCommands.append({name, category, action});
}

void CommandPalette::show()
{
    mSearchBox->clear();
    filterCommands("");

    QWidget *pw = qobject_cast<QWidget*>(parent());
    if (pw) {
        QPoint center = pw->mapToGlobal(pw->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2 - 80);
    }

    QWidget::show();
    mSearchBox->setFocus();
    raise();
}

void CommandPalette::filterCommands(const QString &text)
{
    mResultsList->clear();

    for (const CommandItem &cmd : mCommands) {
        if (text.isEmpty() || cmd.name.contains(text, Qt::CaseInsensitive) ||
            cmd.category.contains(text, Qt::CaseInsensitive)) {
            auto *item = new QListWidgetItem(
                QString("%1  \u2014  %2").arg(cmd.category, cmd.name));
            item->setData(Qt::UserRole, mResultsList->count());
            mResultsList->addItem(item);
        }
    }

    if (mResultsList->count() > 0)
        mResultsList->setCurrentRow(0);

    int itemHeight = 36;
    int visibleItems = qMin(mResultsList->count(), 9);
    mResultsList->setFixedHeight(visibleItems * itemHeight + 4);
    adjustSize();
}

void CommandPalette::executeSelected()
{
    QListWidgetItem *item = mResultsList->currentItem();
    if (!item)
        return;

    int matchIndex = 0;
    QString text = mSearchBox->text();
    for (int i = 0; i < mCommands.size(); ++i) {
        const CommandItem &cmd = mCommands.at(i);
        if (text.isEmpty() || cmd.name.contains(text, Qt::CaseInsensitive) ||
            cmd.category.contains(text, Qt::CaseInsensitive)) {
            if (matchIndex == mResultsList->currentRow()) {
                hide();
                cmd.action();
                return;
            }
            matchIndex++;
        }
    }
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == mSearchBox && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);

        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            executeSelected();
            return true;
        case Qt::Key_Up:
            if (mResultsList->currentRow() > 0)
                mResultsList->setCurrentRow(mResultsList->currentRow() - 1);
            return true;
        case Qt::Key_Down:
            if (mResultsList->currentRow() < mResultsList->count() - 1)
                mResultsList->setCurrentRow(mResultsList->currentRow() + 1);
            return true;
        default:
            break;
        }
    }

    return QWidget::eventFilter(obj, event);
}
