/****************************************************************************
** Meta object code from reading C++ file 'sliding_stacked_widget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/sliding_stacked_widget.h"
#include <QtGui/qtextcursor.h>
#include <QScreen>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sliding_stacked_widget.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_SlidingStackedWidget_t {
    uint offsetsAndSizes[34];
    char stringdata0[21];
    char stringdata1[18];
    char stringdata2[1];
    char stringdata3[9];
    char stringdata4[6];
    char stringdata5[13];
    char stringdata6[19];
    char stringdata7[14];
    char stringdata8[16];
    char stringdata9[9];
    char stringdata10[12];
    char stringdata11[12];
    char stringdata12[11];
    char stringdata13[4];
    char stringdata14[12];
    char stringdata15[10];
    char stringdata16[18];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_SlidingStackedWidget_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_SlidingStackedWidget_t qt_meta_stringdata_SlidingStackedWidget = {
    {
        QT_MOC_LITERAL(0, 20),  // "SlidingStackedWidget"
        QT_MOC_LITERAL(21, 17),  // "animationFinished"
        QT_MOC_LITERAL(39, 0),  // ""
        QT_MOC_LITERAL(40, 8),  // "setSpeed"
        QT_MOC_LITERAL(49, 5),  // "speed"
        QT_MOC_LITERAL(55, 12),  // "setAnimation"
        QT_MOC_LITERAL(68, 18),  // "QEasingCurve::Type"
        QT_MOC_LITERAL(87, 13),  // "animationtype"
        QT_MOC_LITERAL(101, 15),  // "setVerticalMode"
        QT_MOC_LITERAL(117, 8),  // "vertical"
        QT_MOC_LITERAL(126, 11),  // "slideInNext"
        QT_MOC_LITERAL(138, 11),  // "slideInPrev"
        QT_MOC_LITERAL(150, 10),  // "slideInIdx"
        QT_MOC_LITERAL(161, 3),  // "idx"
        QT_MOC_LITERAL(165, 11),  // "t_direction"
        QT_MOC_LITERAL(177, 9),  // "direction"
        QT_MOC_LITERAL(187, 17)   // "animationDoneSlot"
    },
    "SlidingStackedWidget",
    "animationFinished",
    "",
    "setSpeed",
    "speed",
    "setAnimation",
    "QEasingCurve::Type",
    "animationtype",
    "setVerticalMode",
    "vertical",
    "slideInNext",
    "slideInPrev",
    "slideInIdx",
    "idx",
    "t_direction",
    "direction",
    "animationDoneSlot"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_SlidingStackedWidget[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   74,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    1,   75,    2, 0x0a,    2 /* Public */,
       5,    1,   78,    2, 0x0a,    4 /* Public */,
       8,    1,   81,    2, 0x0a,    6 /* Public */,
       8,    0,   84,    2, 0x2a,    8 /* Public | MethodCloned */,
      10,    0,   85,    2, 0x0a,    9 /* Public */,
      11,    0,   86,    2, 0x0a,   10 /* Public */,
      12,    2,   87,    2, 0x0a,   11 /* Public */,
      12,    1,   92,    2, 0x2a,   14 /* Public | MethodCloned */,
      16,    0,   95,    2, 0x08,   16 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 14,   13,   15,
    QMetaType::Void, QMetaType::Int,   13,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject SlidingStackedWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QStackedWidget::staticMetaObject>(),
    qt_meta_stringdata_SlidingStackedWidget.offsetsAndSizes,
    qt_meta_data_SlidingStackedWidget,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_SlidingStackedWidget_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SlidingStackedWidget, std::true_type>,
        // method 'animationFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setSpeed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setAnimation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QEasingCurve::Type, std::false_type>,
        // method 'setVerticalMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setVerticalMode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'slideInNext'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'slideInPrev'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'slideInIdx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<t_direction, std::false_type>,
        // method 'slideInIdx'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'animationDoneSlot'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void SlidingStackedWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SlidingStackedWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->animationFinished(); break;
        case 1: _t->setSpeed((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->setAnimation((*reinterpret_cast< std::add_pointer_t<QEasingCurve::Type>>(_a[1]))); break;
        case 3: _t->setVerticalMode((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->setVerticalMode(); break;
        case 5: _t->slideInNext(); break;
        case 6: _t->slideInPrev(); break;
        case 7: _t->slideInIdx((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<t_direction>>(_a[2]))); break;
        case 8: _t->slideInIdx((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->animationDoneSlot(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SlidingStackedWidget::*)();
            if (_t _q_method = &SlidingStackedWidget::animationFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *SlidingStackedWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SlidingStackedWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SlidingStackedWidget.stringdata0))
        return static_cast<void*>(this);
    return QStackedWidget::qt_metacast(_clname);
}

int SlidingStackedWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStackedWidget::qt_metacall(_c, _id, _a);
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

// SIGNAL 0
void SlidingStackedWidget::animationFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
