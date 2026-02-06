/****************************************************************************
** Meta object code from reading C++ file 'processes_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Processes/processes_page.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'processes_page.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_ProcessesPage_t {
    uint offsetsAndSizes[32];
    char stringdata0[14];
    char stringdata1[5];
    char stringdata2[1];
    char stringdata3[14];
    char stringdata4[15];
    char stringdata5[10];
    char stringdata6[22];
    char stringdata7[8];
    char stringdata8[5];
    char stringdata9[32];
    char stringdata10[4];
    char stringdata11[30];
    char stringdata12[2];
    char stringdata13[25];
    char stringdata14[43];
    char stringdata15[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_ProcessesPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_ProcessesPage_t qt_meta_stringdata_ProcessesPage = {
    {
        QT_MOC_LITERAL(0, 13),  // "ProcessesPage"
        QT_MOC_LITERAL(14, 4),  // "init"
        QT_MOC_LITERAL(19, 0),  // ""
        QT_MOC_LITERAL(20, 13),  // "loadProcesses"
        QT_MOC_LITERAL(34, 14),  // "loadHeaderMenu"
        QT_MOC_LITERAL(49, 9),  // "createRow"
        QT_MOC_LITERAL(59, 21),  // "QList<QStandardItem*>"
        QT_MOC_LITERAL(81, 7),  // "Process"
        QT_MOC_LITERAL(89, 4),  // "proc"
        QT_MOC_LITERAL(94, 31),  // "on_txtProcessSearch_textChanged"
        QT_MOC_LITERAL(126, 3),  // "val"
        QT_MOC_LITERAL(130, 29),  // "on_sliderRefresh_valueChanged"
        QT_MOC_LITERAL(160, 1),  // "i"
        QT_MOC_LITERAL(162, 24),  // "on_btnEndProcess_clicked"
        QT_MOC_LITERAL(187, 42),  // "on_tableProcess_customContext..."
        QT_MOC_LITERAL(230, 3)   // "pos"
    },
    "ProcessesPage",
    "init",
    "",
    "loadProcesses",
    "loadHeaderMenu",
    "createRow",
    "QList<QStandardItem*>",
    "Process",
    "proc",
    "on_txtProcessSearch_textChanged",
    "val",
    "on_sliderRefresh_valueChanged",
    "i",
    "on_btnEndProcess_clicked",
    "on_tableProcess_customContextMenuRequested",
    "pos"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_ProcessesPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   62,    2, 0x08,    1 /* Private */,
       3,    0,   63,    2, 0x08,    2 /* Private */,
       4,    0,   64,    2, 0x08,    3 /* Private */,
       5,    1,   65,    2, 0x08,    4 /* Private */,
       9,    1,   68,    2, 0x08,    6 /* Private */,
      11,    1,   71,    2, 0x08,    8 /* Private */,
      13,    0,   74,    2, 0x08,   10 /* Private */,
      14,    1,   75,    2, 0x08,   11 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    0x80000000 | 6, 0x80000000 | 7,    8,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,   15,

       0        // eod
};

Q_CONSTINIT const QMetaObject ProcessesPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ProcessesPage.offsetsAndSizes,
    qt_meta_data_ProcessesPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_ProcessesPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ProcessesPage, std::true_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadProcesses'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadHeaderMenu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'createRow'
        QtPrivate::TypeAndForceComplete<QList<QStandardItem*>, std::false_type>,
        QtPrivate::TypeAndForceComplete<const Process &, std::false_type>,
        // method 'on_txtProcessSearch_textChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'on_sliderRefresh_valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const int &, std::false_type>,
        // method 'on_btnEndProcess_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_tableProcess_customContextMenuRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>
    >,
    nullptr
} };

void ProcessesPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ProcessesPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->init(); break;
        case 1: _t->loadProcesses(); break;
        case 2: _t->loadHeaderMenu(); break;
        case 3: { QList<QStandardItem*> _r = _t->createRow((*reinterpret_cast< std::add_pointer_t<Process>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QList<QStandardItem*>*>(_a[0]) = std::move(_r); }  break;
        case 4: _t->on_txtProcessSearch_textChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->on_sliderRefresh_valueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->on_btnEndProcess_clicked(); break;
        case 7: _t->on_tableProcess_customContextMenuRequested((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *ProcessesPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ProcessesPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ProcessesPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ProcessesPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
