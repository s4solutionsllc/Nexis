/****************************************************************************
** Meta object code from reading C++ file 'feedback.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../stacer/feedback.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'feedback.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_Feedback_t {
    uint offsetsAndSizes[30];
    char stringdata0[9];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[4];
    char stringdata4[13];
    char stringdata5[17];
    char stringdata6[7];
    char stringdata7[16];
    char stringdata8[5];
    char stringdata9[16];
    char stringdata10[19];
    char stringdata11[12];
    char stringdata12[16];
    char stringdata13[15];
    char stringdata14[20];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Feedback_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Feedback_t qt_meta_stringdata_Feedback = {
    {
        QT_MOC_LITERAL(0, 8),  // "Feedback"
        QT_MOC_LITERAL(9, 16),  // "setErrorMessageS"
        QT_MOC_LITERAL(26, 0),  // ""
        QT_MOC_LITERAL(27, 3),  // "msg"
        QT_MOC_LITERAL(31, 12),  // "clearInputsS"
        QT_MOC_LITERAL(44, 16),  // "disableElementsS"
        QT_MOC_LITERAL(61, 6),  // "status"
        QT_MOC_LITERAL(68, 15),  // "setBtnSendTextS"
        QT_MOC_LITERAL(84, 4),  // "text"
        QT_MOC_LITERAL(89, 15),  // "setErrorMessage"
        QT_MOC_LITERAL(105, 18),  // "on_btnSend_clicked"
        QT_MOC_LITERAL(124, 11),  // "clearInputs"
        QT_MOC_LITERAL(136, 15),  // "disableElements"
        QT_MOC_LITERAL(152, 14),  // "setBtnSendText"
        QT_MOC_LITERAL(167, 19)   // "on_btnClose_clicked"
    },
    "Feedback",
    "setErrorMessageS",
    "",
    "msg",
    "clearInputsS",
    "disableElementsS",
    "status",
    "setBtnSendTextS",
    "text",
    "setErrorMessage",
    "on_btnSend_clicked",
    "clearInputs",
    "disableElements",
    "setBtnSendText",
    "on_btnClose_clicked"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Feedback[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   74,    2, 0x06,    1 /* Public */,
       4,    0,   77,    2, 0x06,    3 /* Public */,
       5,    1,   78,    2, 0x06,    4 /* Public */,
       7,    1,   81,    2, 0x06,    6 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       9,    1,   84,    2, 0x08,    8 /* Private */,
      10,    0,   87,    2, 0x08,   10 /* Private */,
      11,    0,   88,    2, 0x08,   11 /* Private */,
      12,    1,   89,    2, 0x08,   12 /* Private */,
      13,    1,   92,    2, 0x08,   14 /* Private */,
      14,    0,   95,    2, 0x08,   16 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    6,
    QMetaType::Void, QMetaType::QString,    8,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    6,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject Feedback::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_Feedback.offsetsAndSizes,
    qt_meta_data_Feedback,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Feedback_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Feedback, std::true_type>,
        // method 'setErrorMessageS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'clearInputsS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'disableElementsS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const bool, std::false_type>,
        // method 'setBtnSendTextS'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setErrorMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'on_btnSend_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clearInputs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'disableElements'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const bool, std::false_type>,
        // method 'setBtnSendText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'on_btnClose_clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void Feedback::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Feedback *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->setErrorMessageS((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->clearInputsS(); break;
        case 2: _t->disableElementsS((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->setBtnSendTextS((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->setErrorMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->on_btnSend_clicked(); break;
        case 6: _t->clearInputs(); break;
        case 7: _t->disableElements((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->setBtnSendText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->on_btnClose_clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Feedback::*)(const QString & );
            if (_t _q_method = &Feedback::setErrorMessageS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Feedback::*)();
            if (_t _q_method = &Feedback::clearInputsS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (Feedback::*)(const bool );
            if (_t _q_method = &Feedback::disableElementsS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (Feedback::*)(const QString & );
            if (_t _q_method = &Feedback::setBtnSendTextS; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *Feedback::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Feedback::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Feedback.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int Feedback::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
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
void Feedback::setErrorMessageS(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Feedback::clearInputsS()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Feedback::disableElementsS(const bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void Feedback::setBtnSendTextS(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
