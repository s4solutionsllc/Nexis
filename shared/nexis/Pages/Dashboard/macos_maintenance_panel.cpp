#include "macos_maintenance_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QPointer>
#include <QtConcurrent>

#include <Utils/command_util.h>

MacOSMaintenancePanel::MacOSMaintenancePanel(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("macOSMaintenancePanel");
    setWindowTitle(tr("macOS Maintenance"));
    setMinimumSize(560, 480);
    buildUI();
}

MacOSMaintenancePanel::~MacOSMaintenancePanel()
{
    // WI-01 UAF-safe backstop: block until every QtConcurrent worker that
    // holds a QPointer<MacOSMaintenancePanel> back to *this* has finished.
    // The QPointer guard in each lambda already drops the GUI callback when
    // the dialog is destroyed; this guarantees the strong invariant even if
    // a worker is mid-execution at destruction time.
    for (auto &row : mRows)
        row.future.waitForFinished();
}

QList<MacOSMaintenanceTask> MacOSMaintenancePanel::defaultTasks()
{
    static const QString kLsregister =
        "/System/Library/Frameworks/CoreServices.framework"
        "/Frameworks/LaunchServices.framework/Support/lsregister";

    return {
        {
            "spotlight_reindex",
            tr("Rebuild Spotlight Index"),
            tr("Deletes and rebuilds the Spotlight search index. Spotlight "
               "will be temporarily unavailable during reindex (30 min–several hours)."),
            tr("Rebuild the Spotlight index on the startup disk?\n\n"
               "Spotlight search will be temporarily unavailable while it rebuilds."),
            "mdutil", {"-E", "/"},
            /*timeoutMs=*/300000, /*needsSudo=*/true,
            /*cmd2=*/{}, /*args2=*/{}, /*timeoutMs2=*/0
        },
        {
            "verify_disk",
            tr("Verify Disk"),
            tr("Checks the integrity of the startup volume. Read-only — does "
               "not modify the disk. Typically takes 1–5 minutes."),
            tr("Verify the integrity of the startup disk?\n\n"
               "This is a read-only check and will not modify any data."),
            "diskutil", {"verifyVolume", "/"},
            /*timeoutMs=*/300000, /*needsSudo=*/false,
            {}, {}, 0
        },
        {
            "rebuild_launch_services",
            tr("Rebuild Launch Services"),
            tr("Rescans the Launch Services database and restarts Finder. "
               "Fixes incorrect default apps and missing 'Open With' entries."),
            tr("Rebuild the Launch Services database and restart Finder?\n\n"
               "This can fix issues with incorrect default application associations."),
            kLsregister,
            {"-r", "-domain", "local", "-domain", "system", "-domain", "user"},
            /*timeoutMs=*/60000, /*needsSudo=*/false,
            "killall", {"Finder"}, /*timeoutMs2=*/5000
        },
        {
            "flush_dns",
            tr("Flush DNS Cache"),
            tr("Clears the DNS resolver cache. Useful after changing DNS "
               "servers or /etc/hosts, or to resolve stale name lookups."),
            tr("Flush the DNS resolver cache?"),
            "dscacheutil", {"-flushcache"},
            /*timeoutMs=*/10000, /*needsSudo=*/true,
            "killall", {"-HUP", "mDNSResponder"}, /*timeoutMs2=*/10000
        },
        {
            "finder_show_hidden",
            tr("Show Hidden Files in Finder"),
            tr("Enables display of hidden files and folders (dotfiles) in "
               "Finder windows. Restarts Finder to apply the change."),
            tr("Enable display of hidden files in Finder?\n\nFinder will be restarted."),
            "defaults",
            {"write", "com.apple.finder", "AppleShowAllFiles", "-bool", "true"},
            /*timeoutMs=*/5000, /*needsSudo=*/false,
            "killall", {"Finder"}, /*timeoutMs2=*/5000
        },
        {
            "finder_show_path_bar",
            tr("Show Path Bar in Finder"),
            tr("Displays the full path bar at the bottom of Finder windows. "
               "Restarts Finder to apply."),
            tr("Enable the Finder path bar?\n\nFinder will be restarted."),
            "defaults",
            {"write", "com.apple.finder", "ShowPathbar", "-bool", "true"},
            /*timeoutMs=*/5000, /*needsSudo=*/false,
            "killall", {"Finder"}, /*timeoutMs2=*/5000
        },
        {
            "finder_show_status_bar",
            tr("Show Status Bar in Finder"),
            tr("Displays the status bar (item count, available disk space) "
               "at the bottom of Finder windows. Restarts Finder to apply."),
            tr("Enable the Finder status bar?\n\nFinder will be restarted."),
            "defaults",
            {"write", "com.apple.finder", "ShowStatusBar", "-bool", "true"},
            /*timeoutMs=*/5000, /*needsSudo=*/false,
            "killall", {"Finder"}, /*timeoutMs2=*/5000
        },
        {
            "finder_quit_menu",
            tr("Add \"Quit Finder\" Menu Item"),
            tr("Adds a Quit option to the Finder application menu, useful "
               "for completely restarting Finder. Restarts Finder to apply."),
            tr("Add a Quit item to the Finder menu?\n\nFinder will be restarted."),
            "defaults",
            {"write", "com.apple.finder", "QuitMenuItem", "-bool", "true"},
            /*timeoutMs=*/5000, /*needsSudo=*/false,
            "killall", {"Finder"}, /*timeoutMs2=*/5000
        }
    };
}

void MacOSMaintenancePanel::buildUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);

    auto *lblTitle = new QLabel(tr("macOS Maintenance"));
    lblTitle->setProperty("accessibleName", "dialog-title");
    mainLayout->addWidget(lblTitle);

    auto *scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea{background-color:transparent;}");
    scrollArea->setWidgetResizable(true);
    scrollArea->viewport()->setAutoFillBackground(false);

    auto *scrollWidget = new QWidget;
    scrollWidget->setStyleSheet("background-color:transparent;");
    auto *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(8);

    const QList<MacOSMaintenanceTask> tasks = defaultTasks();

    for (int i = 0; i < tasks.size(); ++i) {
        const MacOSMaintenanceTask &task = tasks.at(i);

        auto *card = new QWidget;
        card->setObjectName("maintenanceTaskCard");
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(12);

        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(2);

        auto *lblTaskTitle = new QLabel(task.title);
        {
            QFont f = lblTaskTitle->font();
            f.setBold(true);
            lblTaskTitle->setFont(f);
        }
        textCol->addWidget(lblTaskTitle);

        auto *lblDesc = new QLabel(task.description);
        lblDesc->setWordWrap(true);
        lblDesc->setObjectName("maintenanceTaskDesc");
        textCol->addWidget(lblDesc);

        cardLayout->addLayout(textCol, 1);

        auto *rightCol = new QVBoxLayout;
        rightCol->setAlignment(Qt::AlignTop | Qt::AlignRight);
        rightCol->setSpacing(4);

        auto *btn = new QPushButton(tr("Run"));
        btn->setProperty("accessibleName", "primary");
        btn->setFixedWidth(80);
        const int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx]() { runTask(idx); });
        rightCol->addWidget(btn);

        auto *statusLabel = new QLabel;
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setObjectName("maintenanceTaskStatus");
        statusLabel->setWordWrap(true);
        statusLabel->setFixedWidth(80);
        rightCol->addWidget(statusLabel);

        cardLayout->addLayout(rightCol);
        scrollLayout->addWidget(card);

        TaskRow row;
        row.btn = btn;
        row.statusLabel = statusLabel;
        mRows.append(row);
    }

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *btnClose = new QPushButton(tr("Close"));
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(btnClose);
    mainLayout->addLayout(btnRow);
}

void MacOSMaintenancePanel::runTask(int index)
{
    const QList<MacOSMaintenanceTask> tasks = defaultTasks();
    if (index < 0 || index >= tasks.size() || index >= mRows.size())
        return;

    const MacOSMaintenanceTask task = tasks.at(index);
    TaskRow &row = mRows[index];

    if (QMessageBox::question(this, task.title, task.confirmText,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    row.btn->setEnabled(false);
    row.btn->setText(tr("Running…"));
    row.statusLabel->clear();

    // Worker-thread safety contract (mirrors MaintenanceWizardDialog::runChecks):
    //   * QPointer<MacOSMaintenancePanel> guards the GUI-thread callback.
    //   * task captured by value — no member access across threads.
    //   * Result delivered via QMetaObject::invokeMethod (QueuedConnection).
    //   * Destructor waitForFinished() is the backstop.
    QPointer<MacOSMaintenancePanel> self(this);

    row.future = QtConcurrent::run([self, task, index]() {
        ExecResult r;

        if (task.needsSudo)
            r = CommandUtil::sudoExecWithStatus(task.cmd, task.args, {}, task.timeoutMs);
        else
            r = CommandUtil::execWithStatus(task.cmd, task.args, task.timeoutMs);

        // Run optional second command only when the first succeeded
        if (r.ok() && !task.cmd2.isEmpty()) {
            ExecResult r2;
            if (task.needsSudo)
                r2 = CommandUtil::sudoExecWithStatus(task.cmd2, task.args2, {}, task.timeoutMs2);
            else
                r2 = CommandUtil::execWithStatus(task.cmd2, task.args2, task.timeoutMs2);
            if (!r2.ok()) {
                r.exitCode = r2.exitCode;
                r.error    = r2.error.isEmpty() ? r2.output : r2.error;
            }
        }

        const bool  ok  = r.ok();
        const QString out = r.output.trimmed();
        const QString err = r.error.trimmed();

        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, index, ok, out, err]() {
            if (!self) return;
            self->onTaskFinished(index, ok, out, err);
        }, Qt::QueuedConnection);
    });
}

void MacOSMaintenancePanel::onTaskFinished(int index, bool success,
                                            const QString &output,
                                            const QString &error)
{
    if (index < 0 || index >= mRows.size())
        return;

    TaskRow &row = mRows[index];
    row.btn->setEnabled(true);
    row.btn->setText(tr("Run"));

    if (success) {
        QString msg = tr("✓ Done");
        if (!output.isEmpty())
            msg += "\n" + output.left(100);
        row.statusLabel->setText(msg);
        row.statusLabel->setProperty("status", "success");
    } else {
        const QString detail = error.isEmpty() ? output : error;
        QString msg = tr("✗ Failed");
        if (!detail.isEmpty())
            msg += "\n" + detail.left(100);
        row.statusLabel->setText(msg);
        row.statusLabel->setProperty("status", "error");
    }

    row.statusLabel->style()->unpolish(row.statusLabel);
    row.statusLabel->style()->polish(row.statusLabel);
}
