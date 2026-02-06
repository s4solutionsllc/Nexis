/****************************************************************************
** Meta object code from reading C++ file 'startup_app_edit.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/Pages/StartupApps/startup_app_edit.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'startup_app_edit.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_StartupAppEdit_t {
    uint offsetsAndSizes[24];
    char stringdata0[15];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[5];
    char stringdata5[8];
    char stringdata6[19];
    char stringdata7[19];
    char stringdata8[13];
    char stringdata9[6];
    char stringdata10[4];
    char stringdata11[5];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_StartupAppEdit_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_StartupAppEdit_t qt_meta_stringdata_StartupAppEdit = {
    {
        QT_MOC_LITERAL(0, 14),  // "StartupAppEdit"
        QT_MOC_LITERAL(15, 15),  // "startupAppAdded"
        QT_MOC_LITERAL(31, 0),  // ""
        QT_MOC_LITERAL(32, 4),  // "show"
        QT_MOC_LITERAL(37, 4),  // "init"
        QT_MOC_LITERAL(42, 7),  // "isValid"
        QT_MOC_LITERAL(50, 18),  // "on_btnSave_clicked"
        QT_MOC_LITERAL(69, 18),  // "changeDesktopValue"
        QT_MOC_LITERAL(88, 12),  // "QStringList&"
        QT_MOC_LITERAL(101, 5),  // "lines"
        QT_MOC_LITERAL(107, 3),  // "reg"
        QT_MOC_LITERAL(111, 4)   // "text"
    },
    "StartupAppEdit",
    "startupAppAdded",
    "",
    "show",
    "init",
    "isValid",
    "on_btnSave_clicked",
    "changeDesktopValue",
    "QStringList&",
    "lines",
    "reg",
    "text"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_StartupAppEdit[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   51,    2, 0x0a,    2 /* Public */,
       4,    0,   52,    2, 0x08,    3 /* Private */,
       5,    0,   53,    2, 0x08,    4 /* Private */,
       6,    0,   54,    2, 0x08,    5 /* Private */,
       7,    3,   55,    2, 0x08,    6 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8, QMetaType::QRegularExpression, QMetaType::QString,    9,   10,   11,

       0        // eod
};

Q_CONSTINIT const QMetaObject StartupAppEdit::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_StartupAppEdit.offsetsAndSizes,
    qt_meta_data_StartupAppEdit,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_StartupAppEdit_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<StartupAppEdit, std::true_type>,
        // method 'startupAppAdded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'show'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'init'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isValid'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'on_btnSave_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'changeDesktopValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRegularExpression &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void StartupAppEdit::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StartupAppEdit *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->startupAppAdded(); break;
        case 1: _t->show(); break;
        case 2: _t->init(); break;
        case 3: { bool _r = _t->isValid();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 4: _t->on_btnSave_clicked(); break;
        case 5: _t->changeDesktopValue((*reinterpret_cast< std::add_pointer_t<QStringList&>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRegularExpression>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (StartupAppEdit::*)();
            if (_t _q_method = &StartupAppEdit::startupAppAdded; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *StartupAppEdit::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StartupAppEdit::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StartupAppEdit.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int StartupAppEdit::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void StartupAppEdit::startupAppAdded()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
