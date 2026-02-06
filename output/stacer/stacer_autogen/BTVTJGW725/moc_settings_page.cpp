/****************************************************************************
** Meta object code from reading C++ file 'settings_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Settings/settings_page.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'settings_page.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_SettingsPage_t {
    uint offsetsAndSizes[32];
    char stringdata0[13];
    char stringdata1[5];
    char stringdata2[1];
    char stringdata3[20];
    char stringdata4[6];
    char stringdata5[15];
    char stringdata6[26];
    char stringdata7[8];
    char stringdata8[21];
    char stringdata9[20];
    char stringdata10[5];
    char stringdata11[31];
    char stringdata12[6];
    char stringdata13[34];
    char stringdata14[32];
    char stringdata15[31];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_SettingsPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_SettingsPage_t qt_meta_stringdata_SettingsPage = {
    {
        QT_MOC_LITERAL(0, 12),  // "SettingsPage"
        QT_MOC_LITERAL(13, 4),  // "init"
        QT_MOC_LITERAL(18, 0),  // ""
        QT_MOC_LITERAL(19, 19),  // "cmbLanguagesChanged"
        QT_MOC_LITERAL(39, 5),  // "index"
        QT_MOC_LITERAL(45, 14),  // "cmbDiskChanged"
        QT_MOC_LITERAL(60, 25),  // "on_checkAutostart_clicked"
        QT_MOC_LITERAL(86, 7),  // "checked"
        QT_MOC_LITERAL(94, 20),  // "on_btnDonate_clicked"
        QT_MOC_LITERAL(115, 19),  // "cmbStartPageChanged"
        QT_MOC_LITERAL(135, 4),  // "text"
        QT_MOC_LITERAL(140, 30),  // "on_spinCpuPercent_valueChanged"
        QT_MOC_LITERAL(171, 5),  // "value"
        QT_MOC_LITERAL(177, 33),  // "on_spinMemoryPercent_valueCha..."
        QT_MOC_LITERAL(211, 31),  // "on_spinDiskPercent_valueChanged"
        QT_MOC_LITERAL(243, 30)   // "on_checkAppQuitDontAsk_clicked"
    },
    "SettingsPage",
    "init",
    "",
    "cmbLanguagesChanged",
    "index",
    "cmbDiskChanged",
    "on_checkAutostart_clicked",
    "checked",
    "on_btnDonate_clicked",
    "cmbStartPageChanged",
    "text",
    "on_spinCpuPercent_valueChanged",
    "value",
    "on_spinMemoryPercent_valueChanged",
    "on_spinDiskPercent_valueChanged",
    "on_checkAppQuitDontAsk_clicked"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_SettingsPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   74,    2, 0x08,    1 /* Private */,
       3,    1,   75,    2, 0x08,    2 /* Private */,
       5,    1,   78,    2, 0x08,    4 /* Private */,
       6,    1,   81,    2, 0x08,    6 /* Private */,
       8,    0,   84,    2, 0x08,    8 /* Private */,
       9,    1,   85,    2, 0x08,    9 /* Private */,
      11,    1,   88,    2, 0x08,   11 /* Private */,
      13,    1,   91,    2, 0x08,   13 /* Private */,
      14,    1,   94,    2, 0x08,   15 /* Private */,
      15,    1,   97,    2, 0x08,   17 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Bool,    7,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Bool,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject SettingsPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_SettingsPage.offsetsAndSizes,
    qt_meta_data_SettingsPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_SettingsPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SettingsPage, std::true_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cmbLanguagesChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const int &, std::false_type>,
        // method 'cmbDiskChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const int &, std::false_type>,
        // method 'on_checkAutostart_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'on_btnDonate_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cmbStartPageChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString, std::false_type>,
        // method 'on_spinCpuPercent_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_spinMemoryPercent_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_spinDiskPercent_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_checkAppQuitDontAsk_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void SettingsPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SettingsPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->init(); break;
        case 1: _t->cmbLanguagesChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->cmbDiskChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->on_checkAutostart_clicked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->on_btnDonate_clicked(); break;
        case 5: _t->cmbStartPageChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->on_spinCpuPercent_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->on_spinMemoryPercent_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->on_spinDiskPercent_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->on_checkAppQuitDontAsk_clicked((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *SettingsPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SettingsPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SettingsPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SettingsPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
