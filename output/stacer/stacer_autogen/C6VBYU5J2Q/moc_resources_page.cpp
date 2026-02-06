/****************************************************************************
** Meta object code from reading C++ file 'resources_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Resources/resources_page.h"
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
#error "The header file 'resources_page.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_ResourcesPage_t {
    uint offsetsAndSizes[18];
    char stringdata0[14];
    char stringdata1[15];
    char stringdata2[1];
    char stringdata3[17];
    char stringdata4[20];
    char stringdata5[18];
    char stringdata6[19];
    char stringdata7[17];
    char stringdata8[23];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_ResourcesPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_ResourcesPage_t qt_meta_stringdata_ResourcesPage = {
    {
        QT_MOC_LITERAL(0, 13),  // "ResourcesPage"
        QT_MOC_LITERAL(14, 14),  // "updateCpuChart"
        QT_MOC_LITERAL(29, 0),  // ""
        QT_MOC_LITERAL(30, 16),  // "updateCpuLoadAvg"
        QT_MOC_LITERAL(47, 19),  // "updateDiskReadWrite"
        QT_MOC_LITERAL(67, 17),  // "updateMemoryChart"
        QT_MOC_LITERAL(85, 18),  // "updateNetworkChart"
        QT_MOC_LITERAL(104, 16),  // "initDiskPieChart"
        QT_MOC_LITERAL(121, 22)   // "diskPieSeriesCustomize"
    },
    "ResourcesPage",
    "updateCpuChart",
    "",
    "updateCpuLoadAvg",
    "updateDiskReadWrite",
    "updateMemoryChart",
    "updateNetworkChart",
    "initDiskPieChart",
    "diskPieSeriesCustomize"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_ResourcesPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   56,    2, 0x08,    1 /* Private */,
       3,    0,   57,    2, 0x08,    2 /* Private */,
       4,    0,   58,    2, 0x08,    3 /* Private */,
       5,    0,   59,    2, 0x08,    4 /* Private */,
       6,    0,   60,    2, 0x08,    5 /* Private */,
       7,    0,   61,    2, 0x08,    6 /* Private */,
       8,    0,   62,    2, 0x08,    7 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject ResourcesPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ResourcesPage.offsetsAndSizes,
    qt_meta_data_ResourcesPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_ResourcesPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ResourcesPage, std::true_type>,
        // method 'updateCpuChart'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateCpuLoadAvg'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateDiskReadWrite'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateMemoryChart'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateNetworkChart'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'initDiskPieChart'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'diskPieSeriesCustomize'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ResourcesPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ResourcesPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->updateCpuChart(); break;
        case 1: _t->updateCpuLoadAvg(); break;
        case 2: _t->updateDiskReadWrite(); break;
        case 3: _t->updateMemoryChart(); break;
        case 4: _t->updateNetworkChart(); break;
        case 5: _t->initDiskPieChart(); break;
        case 6: _t->diskPieSeriesCustomize(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *ResourcesPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ResourcesPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ResourcesPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ResourcesPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
