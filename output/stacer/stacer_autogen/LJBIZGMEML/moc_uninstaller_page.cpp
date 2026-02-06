/****************************************************************************
** Meta object code from reading C++ file 'uninstaller_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Uninstaller/uninstaller_page.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uninstaller_page.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_UninstallerPage_t {
    uint offsetsAndSizes[42];
    char stringdata0[16];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[20];
    char stringdata4[17];
    char stringdata5[12];
    char stringdata6[32];
    char stringdata7[4];
    char stringdata8[24];
    char stringdata9[20];
    char stringdata10[24];
    char stringdata11[14];
    char stringdata12[18];
    char stringdata13[17];
    char stringdata14[21];
    char stringdata15[29];
    char stringdata16[27];
    char stringdata17[38];
    char stringdata18[17];
    char stringdata19[5];
    char stringdata20[34];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_UninstallerPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_UninstallerPage_t qt_meta_stringdata_UninstallerPage = {
    {
        QT_MOC_LITERAL(0, 15),  // "UninstallerPage"
        QT_MOC_LITERAL(16, 15),  // "packagesLoadedS"
        QT_MOC_LITERAL(32, 0),  // ""
        QT_MOC_LITERAL(33, 19),  // "snapPackagesLoadedS"
        QT_MOC_LITERAL(53, 16),  // "uninstallStarted"
        QT_MOC_LITERAL(70, 11),  // "setAppCount"
        QT_MOC_LITERAL(82, 31),  // "on_txtPackageSearch_textChanged"
        QT_MOC_LITERAL(114, 3),  // "val"
        QT_MOC_LITERAL(118, 23),  // "on_btnUninstall_clicked"
        QT_MOC_LITERAL(142, 19),  // "getSelectedPackages"
        QT_MOC_LITERAL(162, 23),  // "getSelectedSnapPackages"
        QT_MOC_LITERAL(186, 13),  // "fetchPackages"
        QT_MOC_LITERAL(200, 17),  // "fetchSnapPackages"
        QT_MOC_LITERAL(218, 16),  // "onPackagesLoaded"
        QT_MOC_LITERAL(235, 20),  // "onSnapPackagesLoaded"
        QT_MOC_LITERAL(256, 28),  // "on_btnSystemPackages_clicked"
        QT_MOC_LITERAL(285, 26),  // "on_btnSnapPackages_clicked"
        QT_MOC_LITERAL(312, 37),  // "on_listWidgetSnapPackages_ite..."
        QT_MOC_LITERAL(350, 16),  // "QListWidgetItem*"
        QT_MOC_LITERAL(367, 4),  // "item"
        QT_MOC_LITERAL(372, 33)   // "on_listWidgetPackages_itemCli..."
    },
    "UninstallerPage",
    "packagesLoadedS",
    "",
    "snapPackagesLoadedS",
    "uninstallStarted",
    "setAppCount",
    "on_txtPackageSearch_textChanged",
    "val",
    "on_btnUninstall_clicked",
    "getSelectedPackages",
    "getSelectedSnapPackages",
    "fetchPackages",
    "fetchSnapPackages",
    "onPackagesLoaded",
    "onSnapPackagesLoaded",
    "on_btnSystemPackages_clicked",
    "on_btnSnapPackages_clicked",
    "on_listWidgetSnapPackages_itemClicked",
    "QListWidgetItem*",
    "item",
    "on_listWidgetPackages_itemClicked"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_UninstallerPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  110,    2, 0x06,    1 /* Public */,
       3,    0,  111,    2, 0x06,    2 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       4,    0,  112,    2, 0x0a,    3 /* Public */,
       5,    0,  113,    2, 0x08,    4 /* Private */,
       6,    1,  114,    2, 0x08,    5 /* Private */,
       8,    0,  117,    2, 0x08,    7 /* Private */,
       9,    0,  118,    2, 0x08,    8 /* Private */,
      10,    0,  119,    2, 0x08,    9 /* Private */,
      11,    0,  120,    2, 0x08,   10 /* Private */,
      12,    0,  121,    2, 0x08,   11 /* Private */,
      13,    0,  122,    2, 0x08,   12 /* Private */,
      14,    0,  123,    2, 0x08,   13 /* Private */,
      15,    0,  124,    2, 0x08,   14 /* Private */,
      16,    0,  125,    2, 0x08,   15 /* Private */,
      17,    1,  126,    2, 0x08,   16 /* Private */,
      20,    1,  129,    2, 0x08,   18 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void,
    QMetaType::QStringList,
    QMetaType::QStringList,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 18,   19,
    QMetaType::Void, 0x80000000 | 18,   19,

       0        // eod
};

Q_CONSTINIT const QMetaObject UninstallerPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_UninstallerPage.offsetsAndSizes,
    qt_meta_data_UninstallerPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_UninstallerPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<UninstallerPage, std::true_type>,
        // method 'packagesLoadedS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'snapPackagesLoadedS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'uninstallStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setAppCount'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_txtPackageSearch_textChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'on_btnUninstall_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getSelectedPackages'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        // method 'getSelectedSnapPackages'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        // method 'fetchPackages'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchSnapPackages'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPackagesLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSnapPackagesLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_btnSystemPackages_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_btnSnapPackages_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_listWidgetSnapPackages_itemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'on_listWidgetPackages_itemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>
    >,
    nullptr
} };

void UninstallerPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<UninstallerPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->packagesLoadedS(); break;
        case 1: _t->snapPackagesLoadedS(); break;
        case 2: _t->uninstallStarted(); break;
        case 3: _t->setAppCount(); break;
        case 4: _t->on_txtPackageSearch_textChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->on_btnUninstall_clicked(); break;
        case 6: { QStringList _r = _t->getSelectedPackages();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 7: { QStringList _r = _t->getSelectedSnapPackages();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->fetchPackages(); break;
        case 9: _t->fetchSnapPackages(); break;
        case 10: _t->onPackagesLoaded(); break;
        case 11: _t->onSnapPackagesLoaded(); break;
        case 12: _t->on_btnSystemPackages_clicked(); break;
        case 13: _t->on_btnSnapPackages_clicked(); break;
        case 14: _t->on_listWidgetSnapPackages_itemClicked((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 15: _t->on_listWidgetPackages_itemClicked((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (UninstallerPage::*)();
            if (_t _q_method = &UninstallerPage::packagesLoadedS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (UninstallerPage::*)();
            if (_t _q_method = &UninstallerPage::snapPackagesLoadedS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *UninstallerPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UninstallerPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_UninstallerPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UninstallerPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void UninstallerPage::packagesLoadedS()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void UninstallerPage::snapPackagesLoadedS()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
