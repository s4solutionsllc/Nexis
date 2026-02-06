/****************************************************************************
** Meta object code from reading C++ file 'services_page.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/Services/services_page.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'services_page.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_ServicesPage_t {
    uint offsetsAndSizes[20];
    char stringdata0[13];
    char stringdata1[14];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[12];
    char stringdata5[13];
    char stringdata6[40];
    char stringdata7[6];
    char stringdata8[40];
    char stringdata9[16];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_ServicesPage_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_ServicesPage_t qt_meta_stringdata_ServicesPage = {
    {
        QT_MOC_LITERAL(0, 12),  // "ServicesPage"
        QT_MOC_LITERAL(13, 13),  // "loadServicesS"
        QT_MOC_LITERAL(27, 0),  // ""
        QT_MOC_LITERAL(28, 4),  // "init"
        QT_MOC_LITERAL(33, 11),  // "getServices"
        QT_MOC_LITERAL(45, 12),  // "loadServices"
        QT_MOC_LITERAL(58, 39),  // "on_cmbRunningStatus_currentIn..."
        QT_MOC_LITERAL(98, 5),  // "index"
        QT_MOC_LITERAL(104, 39),  // "on_cmbStartupStatus_currentIn..."
        QT_MOC_LITERAL(144, 15)   // "setServiceCount"
    },
    "ServicesPage",
    "loadServicesS",
    "",
    "init",
    "getServices",
    "loadServices",
    "on_cmbRunningStatus_currentIndexChanged",
    "index",
    "on_cmbStartupStatus_currentIndexChanged",
    "setServiceCount"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_ServicesPage[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   56,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   57,    2, 0x08,    2 /* Private */,
       4,    0,   58,    2, 0x08,    3 /* Private */,
       5,    0,   59,    2, 0x08,    4 /* Private */,
       6,    1,   60,    2, 0x08,    5 /* Private */,
       8,    1,   63,    2, 0x08,    7 /* Private */,
       9,    0,   66,    2, 0x0a,    9 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject ServicesPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ServicesPage.offsetsAndSizes,
    qt_meta_data_ServicesPage,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_ServicesPage_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ServicesPage, std::true_type>,
        // method 'loadServicesS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getServices'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadServices'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'on_cmbRunningStatus_currentIndexChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'on_cmbStartupStatus_currentIndexChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setServiceCount'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void ServicesPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ServicesPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->loadServicesS(); break;
        case 1: _t->init(); break;
        case 2: _t->getServices(); break;
        case 3: _t->loadServices(); break;
        case 4: _t->on_cmbRunningStatus_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->on_cmbStartupStatus_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->setServiceCount(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ServicesPage::*)();
            if (_t _q_method = &ServicesPage::loadServicesS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *ServicesPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ServicesPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ServicesPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ServicesPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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

// SIGNAL 0
void ServicesPage::loadServicesS()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
