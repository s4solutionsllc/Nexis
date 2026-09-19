#ifndef NEXIS_PAGE_H
#define NEXIS_PAGE_H

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QVBoxLayout>
#include <QWidget>

class NexisPage : public QWidget
{
    Q_OBJECT

public:
    explicit NexisPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual void onPageActivated() {}
    virtual void onPageDeactivated() {}
};

// Shared page scaffold: every page uses the same gutters and the same header
// anatomy (accent bar + title + optional source line, actions on the right),
// styled by the #sectionHeader* recipe in style.qss.
namespace PageScaffold {

inline QMargins pageMargins() { return QMargins(20, 12, 20, 16); }
inline int pageSpacing() { return 8; }

struct Header {
    QWidget *row = nullptr;
    QLabel *title = nullptr;
    QLabel *source = nullptr;
    QHBoxLayout *layout = nullptr;   // append action widgets here
};

inline Header buildHeader(const QString &title, const QString &source, QWidget *parent,
                          const char *accentToken = "accent")
{
    Header h;
    h.row = new QWidget(parent);
    h.row->setObjectName("sectionHeaderRow");
    h.row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    h.layout = new QHBoxLayout(h.row);
    h.layout->setContentsMargins(0, 0, 0, 0);
    h.layout->setSpacing(8);

    auto *accent = new QFrame(h.row);
    accent->setObjectName("sectionHeaderAccent");
    accent->setProperty("accentToken", accentToken);
    accent->setFrameShape(QFrame::NoFrame);
    accent->setFixedWidth(3);
    accent->setMinimumHeight(26);
    accent->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    h.layout->addWidget(accent);

    auto *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(0);
    h.title = new QLabel(title, h.row);
    h.title->setObjectName("sectionHeaderTitle");
    textCol->addWidget(h.title);
    if (!source.isEmpty()) {
        h.source = new QLabel(source, h.row);
        h.source->setObjectName("sectionHeaderSource");
        textCol->addWidget(h.source);
    }
    h.layout->addLayout(textCol, 1);
    return h;
}

} // namespace PageScaffold

#endif // NEXIS_PAGE_H
