/****************************************************************************
** Meta object code from reading C++ file 'dashboard_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Dashboard/dashboard_page.h"
#include <QtGui/qtextcursor.h>
#include <QScreen>
#include <QtCharts/qlineseries.h>
#include <QtCharts/qabstractbarseries.h>
#include <QtCharts/qvbarmodelmapper.h>
#include <QtCharts/qboxplotseries.h>
#include <QtCharts/qcandlestickseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qpieseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qboxplotseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qpieseries.h>
#include <QtCharts/qpieseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qxyseries.h>
#include <QtCharts/qxyseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qboxplotseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qpieseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCharts/qxyseries.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dashboard_page.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_DashboardPage_t {
    uint offsetsAndSizes[22];
    char stringdata0[14];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[12];
    char stringdata5[22];
    char stringdata6[13];
    char stringdata7[16];
    char stringdata8[14];
    char stringdata9[17];
    char stringdata10[29];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_DashboardPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_DashboardPage_t qt_meta_stringdata_DashboardPage = {
    {
        QT_MOC_LITERAL(0, 13),  // "DashboardPage"
        QT_MOC_LITERAL(14, 16),  // "sigShowUpdateBar"
        QT_MOC_LITERAL(31, 0),  // ""
        QT_MOC_LITERAL(32, 4),  // "init"
        QT_MOC_LITERAL(37, 11),  // "checkUpdate"
        QT_MOC_LITERAL(49, 21),  // "systemInformationInit"
        QT_MOC_LITERAL(71, 12),  // "updateCpuBar"
        QT_MOC_LITERAL(84, 15),  // "updateMemoryBar"
        QT_MOC_LITERAL(100, 13),  // "updateDiskBar"
        QT_MOC_LITERAL(114, 16),  // "updateNetworkBar"
        QT_MOC_LITERAL(131, 28)   // "on_btnDownloadUpdate_clicked"
    },
    "DashboardPage",
    "sigShowUpdateBar",
    "",
    "init",
    "checkUpdate",
    "systemInformationInit",
    "updateCpuBar",
    "updateMemoryBar",
    "updateDiskBar",
    "updateNetworkBar",
    "on_btnDownloadUpdate_clicked"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_DashboardPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   68,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   69,    2, 0x08,    2 /* Private */,
       4,    0,   70,    2, 0x08,    3 /* Private */,
       5,    0,   71,    2, 0x08,    4 /* Private */,
       6,    0,   72,    2, 0x08,    5 /* Private */,
       7,    0,   73,    2, 0x08,    6 /* Private */,
       8,    0,   74,    2, 0x08,    7 /* Private */,
       9,    0,   75,    2, 0x08,    8 /* Private */,
      10,    0,   76,    2, 0x08,    9 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject DashboardPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_DashboardPage.offsetsAndSizes,
    qt_meta_data_DashboardPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_DashboardPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<DashboardPage, std::true_type>,
        // method 'sigShowUpdateBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'checkUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'systemInformationInit'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateCpuBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateMemoryBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateDiskBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateNetworkBar'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_btnDownloadUpdate_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void DashboardPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DashboardPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->sigShowUpdateBar(); break;
        case 1: _t->init(); break;
        case 2: _t->checkUpdate(); break;
        case 3: _t->systemInformationInit(); break;
        case 4: _t->updateCpuBar(); break;
        case 5: _t->updateMemoryBar(); break;
        case 6: _t->updateDiskBar(); break;
        case 7: _t->updateNetworkBar(); break;
        case 8: _t->on_btnDownloadUpdate_clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DashboardPage::*)();
            if (_t _q_method = &DashboardPage::sigShowUpdateBar; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    (void)_a;
}

const QMetaObject *DashboardPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DashboardPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DashboardPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int DashboardPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void DashboardPage::sigShowUpdateBar()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
