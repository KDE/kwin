/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "debug_console.h"
#include "compositor.h"
#include "core/inputdevice.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "internalwindow.h"
#include "keyboard_input.h"
#include "main.h"
#include "opengl/eglbackend.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"
#include "tiles/customtile.h"
#include "tiles/tile.h"
#include "utils/filedescriptor.h"
#include "utils/pipe.h"
#include "virtualdesktops.h"
#include "wayland/abstract_data_source.h"
#include "wayland/clientconnection.h"
#include "wayland/datacontrolsource_v1.h"
#include "wayland/datasource.h"
#include "wayland/display.h"
#include "wayland/primaryselectionsource_v1.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "workspace.h"
#include "xkb.h"
#include <cerrno>
#if KWIN_BUILD_X11
#include "x11window.h"
#endif

#include "ui_debug_console.h"

// frameworks
#include <KLocalizedString>
// Qt
#include <QFont>
#include <QFutureWatcher>
#include <QMetaProperty>
#include <QMetaType>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScopeGuard>
#include <QSortFilterProxyModel>
#include <QWindow>
#include <QtConcurrentRun>

#include <wayland-server-core.h>

// xkb
#include <xkbcommon/xkbcommon.h>

#include <fcntl.h>
#include <functional>
#include <sys/poll.h>

namespace KWin
{

static QString tableHeaderRow(const QString &title)
{
    return QStringLiteral("<tr><th colspan=\"2\">%1</th></tr>").arg(title);
}

template<typename T>
static QString tableRow(const QString &title, const T &argument)
{
    return QStringLiteral("<tr><td>%1</td><td>%2</td></tr>").arg(title).arg(argument);
}

static QString timestampRow(std::chrono::microseconds timestamp)
{
    return tableRow(i18n("Timestamp"), std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

static QString timestampRowUsec(std::chrono::microseconds timestamp)
{
    return tableRow(i18n("Timestamp (µsec)"), timestamp.count());
}

static QString buttonToString(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
        return i18nc("A mouse button", "Left");
    case Qt::RightButton:
        return i18nc("A mouse button", "Right");
    case Qt::MiddleButton:
        return i18nc("A mouse button", "Middle");
    case Qt::BackButton:
        return i18nc("A mouse button", "Back");
    case Qt::ForwardButton:
        return i18nc("A mouse button", "Forward");
    case Qt::TaskButton:
        return i18nc("A mouse button", "Task");
    case Qt::ExtraButton4:
        return i18nc("A mouse button", "Extra Button 4");
    case Qt::ExtraButton5:
        return i18nc("A mouse button", "Extra Button 5");
    case Qt::ExtraButton6:
        return i18nc("A mouse button", "Extra Button 6");
    case Qt::ExtraButton7:
        return i18nc("A mouse button", "Extra Button 7");
    case Qt::ExtraButton8:
        return i18nc("A mouse button", "Extra Button 8");
    case Qt::ExtraButton9:
        return i18nc("A mouse button", "Extra Button 9");
    case Qt::ExtraButton10:
        return i18nc("A mouse button", "Extra Button 10");
    case Qt::ExtraButton11:
        return i18nc("A mouse button", "Extra Button 11");
    case Qt::ExtraButton12:
        return i18nc("A mouse button", "Extra Button 12");
    case Qt::ExtraButton13:
        return i18nc("A mouse button", "Extra Button 13");
    case Qt::ExtraButton14:
        return i18nc("A mouse button", "Extra Button 14");
    case Qt::ExtraButton15:
        return i18nc("A mouse button", "Extra Button 15");
    case Qt::ExtraButton16:
        return i18nc("A mouse button", "Extra Button 16");
    case Qt::ExtraButton17:
        return i18nc("A mouse button", "Extra Button 17");
    case Qt::ExtraButton18:
        return i18nc("A mouse button", "Extra Button 18");
    case Qt::ExtraButton19:
        return i18nc("A mouse button", "Extra Button 19");
    case Qt::ExtraButton20:
        return i18nc("A mouse button", "Extra Button 20");
    case Qt::ExtraButton21:
        return i18nc("A mouse button", "Extra Button 21");
    case Qt::ExtraButton22:
        return i18nc("A mouse button", "Extra Button 22");
    case Qt::ExtraButton23:
        return i18nc("A mouse button", "Extra Button 23");
    case Qt::ExtraButton24:
        return i18nc("A mouse button", "Extra Button 24");
    default:
        return QString();
    }
}

static QString deviceRow(InputDevice *device)
{
    if (!device) {
        return tableRow(i18n("Input Device"), i18nc("The input device of the event is not known", "Unknown"));
    }
    return tableRow(i18n("Input Device"), QStringLiteral("%1 (%2)").arg(device->name(), device->sysPath()));
}

static QString buttonsToString(Qt::MouseButtons buttons)
{
    QString ret;
    for (uint i = 1; i < Qt::ExtraButton24; i = i << 1) {
        if (buttons & i) {
            ret.append(buttonToString(Qt::MouseButton(uint(buttons) & i)));
            ret.append(QStringLiteral(" "));
        }
    };
    return ret.trimmed();
}

static const QString s_hr = QStringLiteral("<hr/>");
static const QString s_tableStart = QStringLiteral("<table>");
static const QString s_tableEnd = QStringLiteral("</table>");

DebugConsoleFilter::DebugConsoleFilter(QTextEdit *textEdit)
    : InputEventSpy()
    , m_textEdit(textEdit)
{
}

DebugConsoleFilter::~DebugConsoleFilter() = default;

void DebugConsoleFilter::pointerMotion(PointerMotionEvent *event)
{
    QString text = s_hr;
    const QString timestamp = timestampRow(event->timestamp);

    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A mouse pointer motion event", "Pointer Motion")));
    text.append(deviceRow(event->device));
    text.append(timestamp);
    text.append(timestampRowUsec(event->timestamp));
    if (!event->delta.isNull()) {
        text.append(tableRow(i18nc("The relative mouse movement", "Delta"),
                             QStringLiteral("%1/%2").arg(event->delta.x()).arg(event->delta.y())));
    }
    if (!event->deltaUnaccelerated.isNull()) {
        text.append(tableRow(i18nc("The relative mouse movement", "Delta (not accelerated)"),
                             QStringLiteral("%1/%2").arg(event->deltaUnaccelerated.x()).arg(event->deltaUnaccelerated.y())));
    }
    text.append(tableRow(i18nc("The global mouse pointer position", "Global Position"), QStringLiteral("%1/%2").arg(event->position.x()).arg(event->position.y())));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pointerButton(PointerButtonEvent *event)
{
    QString text = s_hr;
    const QString timestamp = timestampRow(event->timestamp);

    text.append(s_tableStart);
    if (event->state == PointerButtonState::Pressed) {
        text.append(tableHeaderRow(i18nc("A mouse pointer button press event", "Pointer Button Press")));
        text.append(deviceRow(event->device));
        text.append(timestamp);
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Button"), buttonToString(event->button)));
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Native Button code"), event->nativeButton));
        text.append(tableRow(i18nc("All currently pressed buttons in a mouse press/release event", "Pressed Buttons"), buttonsToString(event->buttons)));
    } else {
        text.append(tableHeaderRow(i18nc("A mouse pointer button release event", "Pointer Button Release")));
        text.append(deviceRow(event->device));
        text.append(timestamp);
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Button"), buttonToString(event->button)));
        text.append(tableRow(i18nc("A button in a mouse press/release event", "Native Button code"), event->nativeButton));
        text.append(tableRow(i18nc("All currently pressed buttons in a mouse press/release event", "Pressed Buttons"), buttonsToString(event->buttons)));
    }
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pointerAxis(PointerAxisEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A mouse pointer axis (wheel) event", "Pointer Axis")));
    text.append(deviceRow(event->device));
    text.append(timestampRow(event->timestamp));
    text.append(tableRow(i18nc("The orientation of a pointer axis event", "Orientation"),
                         event->orientation == Qt::Horizontal ? i18nc("An orientation of a pointer axis event", "Horizontal")
                                                              : i18nc("An orientation of a pointer axis event", "Vertical")));
    text.append(tableRow(i18nc("The angle delta of a pointer axis event", "Delta"), event->delta));
    text.append(tableRow(i18nc("The normalized V120 angle delta of a pointer axis event. V120 is a technical term and shouldn't be changed.", "Delta (V120)"), event->deltaV120));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::keyboardKey(KeyboardKeyEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);

    switch (event->state) {
    case KeyboardKeyState::Repeated:
    case KeyboardKeyState::Pressed:
        text.append(tableHeaderRow(i18nc("A key press event", "Key Press")));
        break;
    case KeyboardKeyState::Released:
        text.append(tableHeaderRow(i18nc("A key release event", "Key Release")));
        break;
    default:
        break;
    }
    text.append(deviceRow(event->device));
    auto modifiersToString = [event] {
        QString ret;
        if (event->modifiers.testFlag(Qt::ShiftModifier)) {
            ret.append(i18nc("A keyboard modifier", "Shift"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers.testFlag(Qt::ControlModifier)) {
            ret.append(i18nc("A keyboard modifier", "Control"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers.testFlag(Qt::AltModifier)) {
            ret.append(i18nc("A keyboard modifier", "Alt"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers.testFlag(Qt::MetaModifier)) {
            ret.append(i18nc("A keyboard modifier", "Meta"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers.testFlag(Qt::KeypadModifier)) {
            ret.append(i18nc("A keyboard modifier", "Keypad"));
            ret.append(QStringLiteral(" "));
        }
        if (event->modifiers.testFlag(Qt::GroupSwitchModifier)) {
            ret.append(i18nc("A keyboard modifier", "Group-switch"));
            ret.append(QStringLiteral(" "));
        }
        return ret;
    };
    text.append(timestampRow(event->timestamp));
    text.append(tableRow(i18nc("Whether the event is an automatic key repeat", "Repeat"), event->state == KeyboardKeyState::Repeated));

    const auto keyMetaObject = Qt::qt_getEnumMetaObject(Qt::Key());
    const auto enumerator = keyMetaObject->enumerator(keyMetaObject->indexOfEnumerator("Key"));
    text.append(tableRow(i18nc("The code reported by the kernel", "Keycode"), event->nativeScanCode));
    text.append(tableRow(i18nc("Key according to Qt", "Qt::Key code"), enumerator.valueToKey(event->key)));
    text.append(tableRow(i18nc("The translated code to an Xkb symbol", "Xkb symbol"), event->nativeVirtualKey));
    text.append(tableRow(i18nc("The translated code interpreted as text", "Utf8"), event->text));
    text.append(tableRow(i18nc("The currently active modifiers", "Modifiers"), modifiersToString()));

    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchDown(TouchDownEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch down event", "Touch down")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), event->id));
    text.append(tableRow(i18nc("The global position of the touch point", "Global position"),
                         QStringLiteral("%1/%2").arg(event->pos.x()).arg(event->pos.y())));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchMotion(TouchMotionEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch motion event", "Touch Motion")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), event->id));
    text.append(tableRow(i18nc("The global position of the touch point", "Global position"),
                         QStringLiteral("%1/%2").arg(event->pos.x()).arg(event->pos.y())));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::touchUp(TouchUpEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A touch up event", "Touch Up")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("The id of the touch point in the touch event", "Point identifier"), event->id));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureBegin(PointerPinchGestureBeginEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture is started", "Pinch start")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("Number of fingers in this pinch gesture", "Finger count"), event->fingerCount));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureUpdate(PointerPinchGestureUpdateEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture is updated", "Pinch update")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("Current scale in pinch gesture", "Scale"), event->scale));
    text.append(tableRow(i18nc("Current angle in pinch gesture", "Angle delta"), event->angleDelta));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta x"), event->delta.x()));
    text.append(tableRow(i18nc("Current delta in pinch gesture", "Delta y"), event->delta.y()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureEnd(PointerPinchGestureEndEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture ended", "Pinch end")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::pinchGestureCancelled(PointerPinchGestureCancelEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A pinch gesture got cancelled", "Pinch cancelled")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture is started", "Swipe start")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("Number of fingers in this swipe gesture", "Finger count"), event->fingerCount));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture is updated", "Swipe update")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta x"), event->delta.x()));
    text.append(tableRow(i18nc("Current delta in swipe gesture", "Delta y"), event->delta.y()));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture ended", "Swipe end")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A swipe gesture got cancelled", "Swipe cancelled")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::holdGestureBegin(PointerHoldGestureBeginEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A hold gesture is started", "Hold start")));
    text.append(timestampRow(event->time));
    text.append(tableRow(i18nc("Number of fingers in this hold gesture", "Finger count"), event->fingerCount));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::holdGestureEnd(PointerHoldGestureEndEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A hold gesture ended", "Hold end")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::holdGestureCancelled(PointerHoldGestureCancelEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A hold gesture got cancelled", "Hold cancelled")));
    text.append(timestampRow(event->time));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::switchEvent(SwitchEvent *event)
{
    QString text = s_hr;
    text.append(s_tableStart);
    text.append(tableHeaderRow(i18nc("A hardware switch (e.g. notebook lid) got toggled", "Switch toggled")));
    text.append(timestampRow(event->timestamp));
    text.append(timestampRowUsec(event->timestamp));
    text.append(deviceRow(event->device));
    QString switchName;
    if (event->device->isLidSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Notebook lid");
    } else if (event->device->isTabletModeSwitch()) {
        switchName = i18nc("Name of a hardware switch", "Tablet mode");
    }
    text.append(tableRow(i18nc("A hardware switch", "Switch"), switchName));
    QString switchState;
    switch (event->state) {
    case SwitchState::Off:
        switchState = i18nc("The hardware switch got turned off", "Off");
        break;
    case SwitchState::On:
        switchState = i18nc("The hardware switch got turned on", "On");
        break;
    default:
        Q_UNREACHABLE();
    }
    text.append(tableRow(i18nc("State of a hardware switch (on/off)", "State"), switchState));
    text.append(s_tableEnd);

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolProximityEvent(TabletToolProximityEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool Proximity"))
        + timestampRow(event->timestamp)
        + timestampRowUsec(event->timestamp)
        + deviceRow(event->device)
        + tableRow(i18n("Proximity"), event->type == TabletToolProximityEvent::EnterProximity ? i18n("In") : i18n("Out"))
        + tableRow(i18n("Position"),
                   QStringLiteral("%1,%2").arg(QString::number(event->position.x()), QString::number(event->position.y())))
        + tableRow(i18n("Tilt"),
                   QStringLiteral("%1,%2").arg(event->xTilt).arg(event->yTilt))
        + tableRow(i18n("Rotation"), QString::number(event->rotation))
        + tableRow(i18n("Distance"), QString::number(event->distance))
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolAxisEvent(TabletToolAxisEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool Axis"))
        + timestampRow(event->timestamp)
        + timestampRowUsec(event->timestamp)
        + deviceRow(event->device)
        + tableRow(i18n("Position"),
                   QStringLiteral("%1,%2").arg(QString::number(event->position.x()), QString::number(event->position.y())))
        + tableRow(i18n("Tilt"),
                   QStringLiteral("%1,%2").arg(event->xTilt).arg(event->yTilt))
        + tableRow(i18n("Rotation"), QString::number(event->rotation))
        + tableRow(i18n("Pressure"), QString::number(event->pressure))
        + tableRow(i18n("Distance"), QString::number(event->distance))
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolTipEvent(TabletToolTipEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool Tip"))
        + timestampRow(event->timestamp)
        + timestampRowUsec(event->timestamp)
        + deviceRow(event->device)
        + tableRow(i18n("Tip"), event->type == TabletToolTipEvent::Press ? i18n("Down") : i18n("Up"))
        + tableRow(i18n("Position"),
                   QStringLiteral("%1,%2").arg(QString::number(event->position.x()), QString::number(event->position.y())))
        + tableRow(i18n("Tilt"),
                   QStringLiteral("%1,%2").arg(event->xTilt).arg(event->yTilt))
        + tableRow(i18n("Rotation"), QString::number(event->rotation))
        + tableRow(i18n("Pressure"), QString::number(event->pressure))
        + tableRow(i18n("Slider Position"), QString::number(event->sliderPosition))
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletToolButtonEvent(TabletToolButtonEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Tool Button"))
        + deviceRow(event->device)
        + tableRow(i18n("Button"), event->button)
        + tableRow(i18n("Pressed"), event->pressed)
        + tableRow(i18n("Tablet"), event->device->name())
        + timestampRow(event->time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadButtonEvent(TabletPadButtonEvent *event)
{
    QString text = s_hr + s_tableStart
        + tableHeaderRow(i18n("Tablet Pad Button"))
        + deviceRow(event->device)
        + tableRow(i18n("Button"), event->button)
        + tableRow(i18n("Pressed"), event->pressed)
        + tableRow(i18n("Group"), event->group)
        + tableRow(i18n("Mode"), event->mode)
        + tableRow(i18n("Is Mode Switch"), event->isModeSwitch)
        + tableRow(i18n("Tablet"), event->device->name())
        + timestampRow(event->time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadStripEvent(TabletPadStripEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Pad Strip"))
        + deviceRow(event->device)
        + tableRow(i18n("Number"), event->number)
        + tableRow(i18n("Position"), event->position)
        + tableRow(i18n("isFinger"), event->isFinger)
        + tableRow(i18n("Group"), event->group)
        + tableRow(i18n("Mode"), event->mode)
        + tableRow(i18n("Tablet"), event->device->name())
        + timestampRow(event->time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadRingEvent(TabletPadRingEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Pad Ring"))
        + deviceRow(event->device)
        + tableRow(i18n("Number"), event->number)
        + tableRow(i18n("Position"), event->position)
        + tableRow(i18n("isFinger"), event->isFinger)
        + tableRow(i18n("Group"), event->group)
        + tableRow(i18n("Mode"), event->mode)
        + tableRow(i18n("Tablet"), event->device->name())
        + timestampRow(event->time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

void DebugConsoleFilter::tabletPadDialEvent(TabletPadDialEvent *event)
{
    QString text = s_hr + s_tableStart + tableHeaderRow(i18n("Tablet Pad Dial"))
        + deviceRow(event->device)
        + tableRow(i18n("Number"), event->number)
        + tableRow(i18n("Delta"), event->delta)
        + tableRow(i18n("Tablet"), event->device->name())
        + timestampRow(event->time)
        + s_tableEnd;

    m_textEdit->insertHtml(text);
    m_textEdit->ensureCursorVisible();
}

static QString sourceString(const AbstractDataSource *const source)
{
    if (!source) {
        return QString();
    }

    if (source->client()) {
        const QString executable = ClientConnection::get(source->client())->executablePath();

        if (auto dataSource = qobject_cast<const DataSourceInterface *const>(source)) {
            return QStringLiteral("wl_data_source@%1 of %2").arg(wl_resource_get_id(dataSource->resource())).arg(executable);
        } else if (qobject_cast<const PrimarySelectionSourceV1Interface *const>(source)) {
            return QStringLiteral("zwp_primary_selection_source_v1 of %1").arg(executable);
        } else if (qobject_cast<const DataControlSourceV1Interface *const>(source)) {
            return QStringLiteral("data control by %1").arg(executable);
        }

        return QStringLiteral("unknown source of").arg(executable);
    }

    return QStringLiteral("%1(0x%2)").arg(source->metaObject()->className()).arg(qulonglong(source), 0, 16);
}

DebugConsole::DebugConsole()
    : QWidget()
    , m_ui(new Ui::DebugConsole)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    m_ui->setupUi(this);

    auto windowsModel = new DebugConsoleModel(this);
    QSortFilterProxyModel *proxyWindowsModel = new QSortFilterProxyModel(this);
    proxyWindowsModel->setSourceModel(windowsModel);
    m_ui->windowsView->setModel(proxyWindowsModel);
    m_ui->windowsView->sortByColumn(0, Qt::AscendingOrder);
    m_ui->windowsView->header()->setSortIndicatorShown(true);
    m_ui->windowsView->setItemDelegate(new DebugConsoleDelegate(this));

    m_ui->clipboardContent->setModel(new DataSourceModel(this));
    m_ui->primaryContent->setModel(new DataSourceModel(this));
    m_ui->inputDevicesView->setModel(new InputDeviceModel(this));
    m_ui->inputDevicesView->setItemDelegate(new DebugConsoleDelegate(this));
    m_ui->tabWidget->setTabIcon(0, QIcon::fromTheme(QStringLiteral("view-list-tree")));

    m_ui->tabWidget->addTab(new DebugConsoleEffectsTab(), i18nc("@label", "Effects"));

    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        // delay creation of input event filter until the tab is selected
        if (index == m_ui->tabWidget->indexOf(m_ui->input) && !m_inputFilter) {
            m_inputFilter = std::make_unique<DebugConsoleFilter>(m_ui->inputTextEdit);
            input()->installInputEventSpy(m_inputFilter.get());
        }
        if (index == m_ui->tabWidget->indexOf(m_ui->keyboard)) {
            updateKeyboardTab();
            connect(input(), &InputRedirection::keyStateChanged, this, &DebugConsole::updateKeyboardTab);
        }
        if (index == m_ui->tabWidget->indexOf(m_ui->clipboard)) {
            static_cast<DataSourceModel *>(m_ui->clipboardContent->model())->setSource(waylandServer()->seat()->selection());
            m_ui->clipboardSource->setText(sourceString(waylandServer()->seat()->selection()));
            connect(waylandServer()->seat(), &SeatInterface::selectionChanged, this, [this](AbstractDataSource *source) {
                static_cast<DataSourceModel *>(m_ui->clipboardContent->model())->setSource(source);
                m_ui->clipboardSource->setText(sourceString(source));
            });
            static_cast<DataSourceModel *>(m_ui->primaryContent->model())->setSource(waylandServer()->seat()->primarySelection());
            m_ui->primarySource->setText(sourceString(waylandServer()->seat()->primarySelection()));
            connect(waylandServer()->seat(), &SeatInterface::primarySelectionChanged, this, [this](AbstractDataSource *source) {
                static_cast<DataSourceModel *>(m_ui->primaryContent->model())->setSource(source);
                m_ui->primarySource->setText(sourceString(source));
            });
        }
    });

    initGLTab();
}

DebugConsole::~DebugConsole() = default;

void DebugConsole::initGLTab()
{
    if (!effects || !effects->isOpenGLCompositing()) {
        m_ui->noOpenGLLabel->setVisible(true);
        m_ui->glInfoScrollArea->setVisible(false);
        return;
    }
    const auto gl = Compositor::self()->scene()->openglContext()->glPlatform();
    m_ui->noOpenGLLabel->setVisible(false);
    m_ui->glInfoScrollArea->setVisible(true);
    m_ui->glVendorStringLabel->setText(QString::fromLocal8Bit(gl->glVendorString()));
    m_ui->glRendererStringLabel->setText(QString::fromLocal8Bit(gl->glRendererString()));
    m_ui->glVersionStringLabel->setText(QString::fromLocal8Bit(gl->glVersionString()));
    m_ui->glslVersionStringLabel->setText(QString::fromLocal8Bit(gl->glShadingLanguageVersionString()));
    m_ui->glDriverLabel->setText(GLPlatform::driverToString(gl->driver()));
    m_ui->glGPULabel->setText(GLPlatform::chipClassToString(gl->chipClass()));
    m_ui->glVersionLabel->setText(gl->glVersion().toString());
    m_ui->glslLabel->setText(gl->glslVersion().toString());

    auto extensionsString = [](const auto &extensions) {
        QString text = QStringLiteral("<ul>");
        for (auto extension : extensions) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(extension)));
        }
        text.append(QStringLiteral("</ul>"));
        return text;
    };

    const EglBackend *backend = static_cast<EglBackend *>(Compositor::self()->backend());
    m_ui->platformExtensionsLabel->setText(extensionsString(backend->extensions()));
    m_ui->openGLExtensionsLabel->setText(extensionsString(backend->openglContext()->openglExtensions()));
}

template<typename T>
QString keymapComponentToString(xkb_keymap *map, const T &count, std::function<const char *(xkb_keymap *, T)> f)
{
    QString text = QStringLiteral("<ul>");
    for (T i = 0; i < count; i++) {
        text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(f(map, i))));
    }
    text.append(QStringLiteral("</ul>"));
    return text;
}

template<typename T>
QString stateActiveComponents(xkb_state *state, const T &count, std::function<int(xkb_state *, T)> f, std::function<const char *(xkb_keymap *, T)> name)
{
    QString text = QStringLiteral("<ul>");
    xkb_keymap *map = xkb_state_get_keymap(state);
    for (T i = 0; i < count; i++) {
        if (f(state, i) == 1) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(name(map, i))));
        }
    }
    text.append(QStringLiteral("</ul>"));
    return text;
}

void DebugConsole::updateKeyboardTab()
{
    auto xkb = input()->keyboard()->xkb();
    xkb_keymap *map = xkb->keymap();
    xkb_state *state = xkb->state();
    m_ui->layoutsLabel->setText(keymapComponentToString<xkb_layout_index_t>(map, xkb_keymap_num_layouts(map), &xkb_keymap_layout_get_name));
    m_ui->currentLayoutLabel->setText(xkb_keymap_layout_get_name(map, xkb->currentLayout()));
    m_ui->modifiersLabel->setText(keymapComponentToString<xkb_mod_index_t>(map, xkb_keymap_num_mods(map), &xkb_keymap_mod_get_name));
    m_ui->ledsLabel->setText(keymapComponentToString<xkb_led_index_t>(map, xkb_keymap_num_leds(map), &xkb_keymap_led_get_name));
    m_ui->activeLedsLabel->setText(stateActiveComponents<xkb_led_index_t>(state, xkb_keymap_num_leds(map), &xkb_state_led_index_is_active, &xkb_keymap_led_get_name));

    using namespace std::placeholders;
    auto modActive = std::bind(xkb_state_mod_index_is_active, _1, _2, XKB_STATE_MODS_EFFECTIVE);
    m_ui->activeModifiersLabel->setText(stateActiveComponents<xkb_mod_index_t>(state, xkb_keymap_num_mods(map), modActive, &xkb_keymap_mod_get_name));
}

void DebugConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // delay the connection to the show event as in ctor the windowHandle returns null
    connect(windowHandle(), &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            // ignore
            return;
        }
        deleteLater();
    });
}

DebugConsoleDelegate::DebugConsoleDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

DebugConsoleDelegate::~DebugConsoleDelegate() = default;

QString DebugConsoleDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    switch (value.userType()) {
    case QMetaType::QPoint: {
        const QPoint p = value.toPoint();
        return QStringLiteral("%1,%2").arg(p.x()).arg(p.y());
    }
    case QMetaType::QPointF: {
        const QPointF p = value.toPointF();
        return QStringLiteral("%1,%2").arg(p.x()).arg(p.y());
    }
    case QMetaType::QSize: {
        const QSize s = value.toSize();
        return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    }
    case QMetaType::QSizeF: {
        const QSizeF s = value.toSizeF();
        return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    }
    case QMetaType::QRect: {
        const QRect r = value.toRect();
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    case QMetaType::QRectF: {
        const QRectF r = value.toRectF();
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    case QMetaType::QIcon: {
        const QIcon icon = value.value<QIcon>();
        if (!icon.isNull()) {
            const auto sizes = icon.availableSizes();

            QStringList sizesStringList;
            sizesStringList.reserve(sizes.size());
            for (const auto &size : sizes) {
                sizesStringList.append(QString::number(size.width()));
            }
            return QStringLiteral("%1 (%2)").arg(icon.name(), displayText(sizesStringList, locale));
        } else {
            return QStringLiteral("null");
        }
    }
    default:
        if (value.userType() == qMetaTypeId<QStringList>()) {
            return value.toStringList().join(QLatin1String(", "));
        }
        if (value.userType() == qMetaTypeId<KWin::SurfaceInterface *>()) {
            if (auto s = value.value<KWin::SurfaceInterface *>()) {
                return QStringLiteral("KWin::SurfaceInterface(0x%1)").arg(qulonglong(s), 0, 16);
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<KWin::Window *>()) {
            if (auto w = value.value<KWin::Window *>()) {
                return w->caption() + QLatin1Char(' ') + QString::fromUtf8(w->metaObject()->className());
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<KWin::LogicalOutput *>()) {
            if (auto output = value.value<KWin::LogicalOutput *>()) {
                return QStringLiteral("%1 (%2@%3x)").arg(output->name(), displayText(output->geometry(), locale), QString::number(output->scale()));
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<KWin::Tile *>()) {
            if (auto tile = value.value<KWin::Tile *>()) {
                const QString tileGeometry = displayText(QVariant::fromValue(tile->absoluteGeometry()), locale);

                if (auto customTile = qobject_cast<KWin::CustomTile *>(tile)) {
                    const QString direction = QMetaEnum::fromType<KWin::Tile::LayoutDirection>().valueToKey(static_cast<quint64>(customTile->layoutDirection()));
                    return QStringLiteral("Custom (%1: %2)").arg(direction, tileGeometry);
                } else {
                    const QString quickTileMode = QMetaEnum::fromType<KWin::QuickTileFlag>().valueToKey(tile->quickTileMode());
                    return QStringLiteral("%1 (%2)").arg(quickTileMode, tileGeometry);
                }
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<KWin::VirtualDesktop *>()) {
            if (auto desktop = value.value<KWin::VirtualDesktop *>()) {
                return desktop->name();
            } else {
                return QStringLiteral("nullptr");
            }
        }
        if (value.userType() == qMetaTypeId<QList<KWin::VirtualDesktop *>>()) {
            const auto desktops = value.value<QList<KWin::VirtualDesktop *>>();

            QStringList result;
            result.reserve(desktops.size());
            for (auto *desktop : desktops) {
                result.append(displayText(QVariant::fromValue(desktop), locale));
            }
            return displayText(result, locale);
        }
        if (value.userType() == qMetaTypeId<Qt::MouseButtons>()) {
            const auto buttons = value.value<Qt::MouseButtons>();
            if (buttons == Qt::NoButton) {
                return i18n("No Mouse Buttons");
            }
            QStringList list;
            if (buttons.testFlag(Qt::LeftButton)) {
                list << i18nc("Mouse Button", "left");
            }
            if (buttons.testFlag(Qt::RightButton)) {
                list << i18nc("Mouse Button", "right");
            }
            if (buttons.testFlag(Qt::MiddleButton)) {
                list << i18nc("Mouse Button", "middle");
            }
            if (buttons.testFlag(Qt::BackButton)) {
                list << i18nc("Mouse Button", "back");
            }
            if (buttons.testFlag(Qt::ForwardButton)) {
                list << i18nc("Mouse Button", "forward");
            }
            if (buttons.testFlag(Qt::ExtraButton1)) {
                list << i18nc("Mouse Button", "extra 1");
            }
            if (buttons.testFlag(Qt::ExtraButton2)) {
                list << i18nc("Mouse Button", "extra 2");
            }
            if (buttons.testFlag(Qt::ExtraButton3)) {
                list << i18nc("Mouse Button", "extra 3");
            }
            if (buttons.testFlag(Qt::ExtraButton4)) {
                list << i18nc("Mouse Button", "extra 4");
            }
            if (buttons.testFlag(Qt::ExtraButton5)) {
                list << i18nc("Mouse Button", "extra 5");
            }
            if (buttons.testFlag(Qt::ExtraButton6)) {
                list << i18nc("Mouse Button", "extra 6");
            }
            if (buttons.testFlag(Qt::ExtraButton7)) {
                list << i18nc("Mouse Button", "extra 7");
            }
            if (buttons.testFlag(Qt::ExtraButton8)) {
                list << i18nc("Mouse Button", "extra 8");
            }
            if (buttons.testFlag(Qt::ExtraButton9)) {
                list << i18nc("Mouse Button", "extra 9");
            }
            if (buttons.testFlag(Qt::ExtraButton10)) {
                list << i18nc("Mouse Button", "extra 10");
            }
            if (buttons.testFlag(Qt::ExtraButton11)) {
                list << i18nc("Mouse Button", "extra 11");
            }
            if (buttons.testFlag(Qt::ExtraButton12)) {
                list << i18nc("Mouse Button", "extra 12");
            }
            if (buttons.testFlag(Qt::ExtraButton13)) {
                list << i18nc("Mouse Button", "extra 13");
            }
            if (buttons.testFlag(Qt::ExtraButton14)) {
                list << i18nc("Mouse Button", "extra 14");
            }
            if (buttons.testFlag(Qt::ExtraButton15)) {
                list << i18nc("Mouse Button", "extra 15");
            }
            if (buttons.testFlag(Qt::ExtraButton16)) {
                list << i18nc("Mouse Button", "extra 16");
            }
            if (buttons.testFlag(Qt::ExtraButton17)) {
                list << i18nc("Mouse Button", "extra 17");
            }
            if (buttons.testFlag(Qt::ExtraButton18)) {
                list << i18nc("Mouse Button", "extra 18");
            }
            if (buttons.testFlag(Qt::ExtraButton19)) {
                list << i18nc("Mouse Button", "extra 19");
            }
            if (buttons.testFlag(Qt::ExtraButton20)) {
                list << i18nc("Mouse Button", "extra 20");
            }
            if (buttons.testFlag(Qt::ExtraButton21)) {
                list << i18nc("Mouse Button", "extra 21");
            }
            if (buttons.testFlag(Qt::ExtraButton22)) {
                list << i18nc("Mouse Button", "extra 22");
            }
            if (buttons.testFlag(Qt::ExtraButton23)) {
                list << i18nc("Mouse Button", "extra 23");
            }
            if (buttons.testFlag(Qt::ExtraButton24)) {
                list << i18nc("Mouse Button", "extra 24");
            }
            if (buttons.testFlag(Qt::TaskButton)) {
                list << i18nc("Mouse Button", "task");
            }
            return list.join(QStringLiteral(", "));
        }
        break;
    }
    return QStyledItemDelegate::displayText(value, locale);
}

static const int s_x11WindowId = 1;
static const int s_x11UnmanagedId = 2;
static const int s_waylandWindowId = 3;
static const int s_workspaceInternalId = 4;
static const quint32 s_propertyBitMask = 0xFFFF0000;
static const quint32 s_windowBitMask = 0x0000FFFF;
static const quint32 s_idDistance = 10000;

template<class T>
void DebugConsoleModel::add(int parentRow, QList<T *> &windows, T *window)
{
    beginInsertRows(index(parentRow, 0, QModelIndex()), windows.count(), windows.count());
    windows.append(window);
    endInsertRows();
}

template<class T>
void DebugConsoleModel::remove(int parentRow, QList<T *> &windows, T *window)
{
    const int remove = windows.indexOf(window);
    if (remove == -1) {
        return;
    }
    beginRemoveRows(index(parentRow, 0, QModelIndex()), remove, remove);
    windows.removeAt(remove);
    endRemoveRows();
}

DebugConsoleModel::DebugConsoleModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    const auto windows = workspace()->windows();
    for (auto window : windows) {
        handleWindowAdded(window);
    }
    connect(workspace(), &Workspace::windowAdded, this, &DebugConsoleModel::handleWindowAdded);
    connect(workspace(), &Workspace::windowRemoved, this, &DebugConsoleModel::handleWindowRemoved);
}

void DebugConsoleModel::handleWindowAdded(Window *window)
{
#if KWIN_BUILD_X11
    if (auto x11 = qobject_cast<X11Window *>(window)) {
        if (x11->isUnmanaged()) {
            add(s_x11UnmanagedId - 1, m_unmanageds, x11);
        } else {
            add(s_x11WindowId - 1, m_x11Windows, x11);
        }
        return;
    }
#endif

    if (auto wayland = qobject_cast<WaylandWindow *>(window)) {
        add(s_waylandWindowId - 1, m_waylandWindows, wayland);
        return;
    }

    if (auto internal = qobject_cast<InternalWindow *>(window)) {
        add(s_workspaceInternalId - 1, m_internalWindows, internal);
        return;
    }
}

void DebugConsoleModel::handleWindowRemoved(Window *window)
{
#if KWIN_BUILD_X11
    if (auto x11 = qobject_cast<X11Window *>(window)) {
        if (x11->isUnmanaged()) {
            remove(s_x11UnmanagedId - 1, m_unmanageds, x11);
        } else {
            remove(s_x11WindowId - 1, m_x11Windows, x11);
        }
        return;
    }
#endif

    if (auto wayland = qobject_cast<WaylandWindow *>(window)) {
        remove(s_waylandWindowId - 1, m_waylandWindows, wayland);
        return;
    }

    if (auto internal = qobject_cast<InternalWindow *>(window)) {
        remove(s_workspaceInternalId - 1, m_internalWindows, internal);
        return;
    }
}

DebugConsoleModel::~DebugConsoleModel() = default;

int DebugConsoleModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

int DebugConsoleModel::topLevelRowCount() const
{
    return 4;
}

template<class T>
int DebugConsoleModel::propertyCount(const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const
{
    if (T *t = (this->*filter)(parent)) {
        return t->metaObject()->propertyCount();
    }
    return 0;
}

int DebugConsoleModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return topLevelRowCount();
    }

    switch (parent.internalId()) {
    case s_x11WindowId:
        return m_x11Windows.count();
    case s_x11UnmanagedId:
        return m_unmanageds.count();
    case s_waylandWindowId:
        return m_waylandWindows.count();
    case s_workspaceInternalId:
        return m_internalWindows.count();
    default:
        break;
    }

    if (parent.internalId() & s_propertyBitMask) {
        // properties do not have children
        return 0;
    }

    if (parent.internalId() < s_idDistance * (s_x11WindowId + 1)) {
#if KWIN_BUILD_X11
        return propertyCount(parent, &DebugConsoleModel::x11Window);
#else
        return 0;
#endif
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
#if KWIN_BUILD_X11
        return propertyCount(parent, &DebugConsoleModel::unmanaged);
#else
        return 0;
#endif
    } else if (parent.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::waylandWindow);
    } else if (parent.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return propertyCount(parent, &DebugConsoleModel::internalWindow);
    }

    return 0;
}

template<class T>
QModelIndex DebugConsoleModel::indexForWindow(int row, int column, const QList<T *> &windows, int id) const
{
    if (column != 0) {
        return QModelIndex();
    }
    if (row >= windows.count()) {
        return QModelIndex();
    }
    return createIndex(row, column, s_idDistance * id + row);
}

template<class T>
QModelIndex DebugConsoleModel::indexForProperty(int row, int column, const QModelIndex &parent, T *(DebugConsoleModel::*filter)(const QModelIndex &) const) const
{
    if (T *t = (this->*filter)(parent)) {
        if (row >= t->metaObject()->propertyCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, quint32(row + 1) << 16 | parent.internalId());
    }
    return QModelIndex();
}

QModelIndex DebugConsoleModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        // index for a top level item
        if (column != 0 || row >= topLevelRowCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, row + 1);
    }
    if (column >= 2) {
        // max of 2 columns
        return QModelIndex();
    }
    // index for a window (second level)
    switch (parent.internalId()) {
    case s_x11WindowId:
        return indexForWindow(row, column, m_x11Windows, s_x11WindowId);
    case s_x11UnmanagedId:
        return indexForWindow(row, column, m_unmanageds, s_x11UnmanagedId);
    case s_waylandWindowId:
        return indexForWindow(row, column, m_waylandWindows, s_waylandWindowId);
    case s_workspaceInternalId:
        return indexForWindow(row, column, m_internalWindows, s_workspaceInternalId);
    default:
        break;
    }

    // index for a property (third level)
    if (parent.internalId() < s_idDistance * (s_x11WindowId + 1)) {
#if KWIN_BUILD_X11
        return indexForProperty(row, column, parent, &DebugConsoleModel::x11Window);
#else
        return {};
#endif
    } else if (parent.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
#if KWIN_BUILD_X11
        return indexForProperty(row, column, parent, &DebugConsoleModel::unmanaged);
#else
        return {};
#endif
    } else if (parent.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::waylandWindow);
    } else if (parent.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return indexForProperty(row, column, parent, &DebugConsoleModel::internalWindow);
    }

    return QModelIndex();
}

QModelIndex DebugConsoleModel::parent(const QModelIndex &child) const
{
    if (child.internalId() <= s_workspaceInternalId) {
        return QModelIndex();
    }
    if (child.internalId() & s_propertyBitMask) {
        // a property
        const quint32 parentId = child.internalId() & s_windowBitMask;
        if (parentId < s_idDistance * (s_x11WindowId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11WindowId), 0, parentId);
        } else if (parentId < s_idDistance * (s_x11UnmanagedId + 1)) {
            return createIndex(parentId - (s_idDistance * s_x11UnmanagedId), 0, parentId);
        } else if (parentId < s_idDistance * (s_waylandWindowId + 1)) {
            return createIndex(parentId - (s_idDistance * s_waylandWindowId), 0, parentId);
        } else if (parentId < s_idDistance * (s_workspaceInternalId + 1)) {
            return createIndex(parentId - (s_idDistance * s_workspaceInternalId), 0, parentId);
        }
        return QModelIndex();
    }
    if (child.internalId() < s_idDistance * (s_x11WindowId + 1)) {
        return createIndex(s_x11WindowId - 1, 0, s_x11WindowId);
    } else if (child.internalId() < s_idDistance * (s_x11UnmanagedId + 1)) {
        return createIndex(s_x11UnmanagedId - 1, 0, s_x11UnmanagedId);
    } else if (child.internalId() < s_idDistance * (s_waylandWindowId + 1)) {
        return createIndex(s_waylandWindowId - 1, 0, s_waylandWindowId);
    } else if (child.internalId() < s_idDistance * (s_workspaceInternalId + 1)) {
        return createIndex(s_workspaceInternalId - 1, 0, s_workspaceInternalId);
    }
    return QModelIndex();
}

QVariant DebugConsoleModel::propertyData(KWin::Window *window, const QModelIndex &index, int role) const
{
    const auto property = window->metaObject()->property(index.row());
    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return property.name();
        } else {
            const QVariant value = property.read(window);
            if (qstrcmp(property.name(), "windowType") == 0) {
                switch (value.toInt()) {
                case NET::Normal:
                    return QStringLiteral("NET::Normal");
                case NET::Desktop:
                    return QStringLiteral("NET::Desktop");
                case NET::Dock:
                    return QStringLiteral("NET::Dock");
                case NET::Toolbar:
                    return QStringLiteral("NET::Toolbar");
                case NET::Menu:
                    return QStringLiteral("NET::Menu");
                case NET::Dialog:
                    return QStringLiteral("NET::Dialog");
                case NET::Override:
                    return QStringLiteral("NET::Override");
                case NET::TopMenu:
                    return QStringLiteral("NET::TopMenu");
                case NET::Utility:
                    return QStringLiteral("NET::Utility");
                case NET::Splash:
                    return QStringLiteral("NET::Splash");
                case NET::DropdownMenu:
                    return QStringLiteral("NET::DropdownMenu");
                case NET::PopupMenu:
                    return QStringLiteral("NET::PopupMenu");
                case NET::Tooltip:
                    return QStringLiteral("NET::Tooltip");
                case NET::Notification:
                    return QStringLiteral("NET::Notification");
                case NET::ComboBox:
                    return QStringLiteral("NET::ComboBox");
                case NET::DNDIcon:
                    return QStringLiteral("NET::DNDIcon");
                case NET::OnScreenDisplay:
                    return QStringLiteral("NET::OnScreenDisplay");
                case NET::CriticalNotification:
                    return QStringLiteral("NET::CriticalNotification");
                case NET::AppletPopup:
                    return QStringLiteral("NET::AppletPopup");
                case NET::Unknown:
                default:
                    return QStringLiteral("NET::Unknown");
                }
            } else if (qstrcmp(property.name(), "layer") == 0) {
                return QMetaEnum::fromType<Layer>().valueToKey(value.value<Layer>());
            }
            return value;
        }
    } else if (role == Qt::DecorationRole) {
        if (index.column() == 1) {
            const QVariant value = property.read(window);
            if (value.userType() == qMetaTypeId<QIcon>()) {
                return value;
            } else if (value.userType() == qMetaTypeId<KWin::Window *>()) {
                if (auto window = value.value<KWin::Window *>()) {
                    return window->icon();
                }
            } else if (qstrcmp(property.name(), "colorScheme") == 0) {
                const QPalette palette = window->palette();

                // Draw a little color scheme preview,
                // inspired by KColorSchemeManagerPrivate::createPreview.
                QPixmap pixmap(16, 16);
                pixmap.fill(Qt::black);
                QPainter painter(&pixmap);
                constexpr int itemSize = 16 / 2 - 1;
                painter.fillRect(1, 1, itemSize, itemSize, palette.window().color());
                painter.fillRect(1 + itemSize, 1, itemSize, itemSize, palette.button().color());
                painter.fillRect(1, 1 + itemSize, itemSize, itemSize, palette.base().color());
                painter.fillRect(1 + itemSize, 1 + itemSize, itemSize, itemSize, palette.highlight().color());
                return pixmap;
            }
        }
    }
    return QVariant();
}

template<class T>
QVariant DebugConsoleModel::windowData(const QModelIndex &index, int role, const QList<T *> windows, const std::function<QString(T *)> &toString) const
{
    if (index.row() >= windows.count()) {
        return QVariant();
    }
    auto c = windows.at(index.row());
    if (role == Qt::DisplayRole) {
        return toString(c);
    } else if (role == Qt::DecorationRole) {
        return c->icon();
    }
    return QVariant();
}

QVariant DebugConsoleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (!index.parent().isValid()) {
        // one of the top levels
        if (index.column() != 0 || role != Qt::DisplayRole) {
            return QVariant();
        }
        switch (index.internalId()) {
        case s_x11WindowId:
            return i18n("X11 Windows");
        case s_x11UnmanagedId:
            return i18n("X11 Unmanaged Windows");
        case s_waylandWindowId:
            return i18n("Wayland Windows");
        case s_workspaceInternalId:
            return i18n("Internal Windows");
        default:
            return QVariant();
        }
    }
    if (index.internalId() & s_propertyBitMask) {
        if (index.column() >= 2) {
            return QVariant();
        }
        if (Window *w = waylandWindow(index)) {
            return propertyData(w, index, role);
        } else if (InternalWindow *w = internalWindow(index)) {
            return propertyData(w, index, role);
#if KWIN_BUILD_X11
        } else if (X11Window *w = x11Window(index)) {
            return propertyData(w, index, role);
        } else if (X11Window *u = unmanaged(index)) {
            return propertyData(u, index, role);
#endif
        }
    } else {
        if (index.column() != 0) {
            return QVariant();
        }

        auto generic = [](Window *c) -> QString {
            return c->caption() + QLatin1Char(' ') + QString::fromUtf8(c->metaObject()->className());
        };
        switch (index.parent().internalId()) {
        case s_x11WindowId:
#if KWIN_BUILD_X11
            return windowData<X11Window>(index, role, m_x11Windows, [](X11Window *c) -> QString {
                return QStringLiteral("0x%1: %2").arg(c->window(), 0, 16).arg(c->caption());
            });
#endif
            break;
        case s_x11UnmanagedId: {
#if KWIN_BUILD_X11
            if (index.row() >= m_unmanageds.count()) {
                return QVariant();
            }
            auto u = m_unmanageds.at(index.row());
            if (role == Qt::DisplayRole) {
                return QStringLiteral("0x%1").arg(u->window(), 0, 16);
            }
#endif
            break;
        }
        case s_waylandWindowId:
            return windowData<WaylandWindow>(index, role, m_waylandWindows, generic);
        case s_workspaceInternalId:
            return windowData<InternalWindow>(index, role, m_internalWindows, generic);
        default:
            break;
        }
    }

    return QVariant();
}

template<class T>
static T *windowForIndex(const QModelIndex &index, const QList<T *> &windows, int id)
{
    const qint32 row = (index.internalId() & s_windowBitMask) - (s_idDistance * id);
    if (row < 0 || row >= windows.count()) {
        return nullptr;
    }
    return windows.at(row);
}

WaylandWindow *DebugConsoleModel::waylandWindow(const QModelIndex &index) const
{
    return windowForIndex(index, m_waylandWindows, s_waylandWindowId);
}

InternalWindow *DebugConsoleModel::internalWindow(const QModelIndex &index) const
{
    return windowForIndex(index, m_internalWindows, s_workspaceInternalId);
}

X11Window *DebugConsoleModel::x11Window(const QModelIndex &index) const
{
    return windowForIndex(index, m_x11Windows, s_x11WindowId);
}

X11Window *DebugConsoleModel::unmanaged(const QModelIndex &index) const
{
    return windowForIndex(index, m_unmanageds, s_x11UnmanagedId);
}

InputDeviceModel::InputDeviceModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_devices(input()->devices())
{
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        setupDeviceConnections(*it);
    }

    connect(input(), &InputRedirection::deviceAdded, this, [this](InputDevice *d) {
        beginInsertRows(QModelIndex(), m_devices.count(), m_devices.count());
        m_devices << d;
        setupDeviceConnections(d);
        endInsertRows();
    });
    connect(input(), &InputRedirection::deviceRemoved, this, [this](InputDevice *d) {
        const int index = m_devices.indexOf(d);
        if (index == -1) {
            return;
        }
        beginRemoveRows(QModelIndex(), index, index);
        m_devices.removeAt(index);
        endRemoveRows();
    });
}

InputDeviceModel::~InputDeviceModel() = default;

int InputDeviceModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant InputDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (!index.parent().isValid() && index.column() == 0) {
        if (index.row() >= m_devices.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return m_devices.at(index.row())->name();
        }
    }
    if (index.parent().isValid()) {
        if (role == Qt::DisplayRole) {
            const auto device = m_devices.at(index.parent().row());
            const auto property = device->metaObject()->property(index.row());
            if (index.column() == 0) {
                return property.name();
            } else if (index.column() == 1) {
                return device->property(property.name());
            }
        }
    }
    return QVariant();
}

QModelIndex InputDeviceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column >= 2) {
        return QModelIndex();
    }
    if (parent.isValid()) {
        if (parent.internalId() & s_propertyBitMask) {
            return QModelIndex();
        }
        if (row >= m_devices.at(parent.row())->metaObject()->propertyCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, quint32(row + 1) << 16 | parent.internalId());
    }
    if (row >= m_devices.count()) {
        return QModelIndex();
    }
    return createIndex(row, column, row + 1);
}

int InputDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_devices.count();
    }
    if (parent.internalId() & s_propertyBitMask) {
        return 0;
    }

    return m_devices.at(parent.row())->metaObject()->propertyCount();
}

QModelIndex InputDeviceModel::parent(const QModelIndex &child) const
{
    if (child.internalId() & s_propertyBitMask) {
        const quintptr parentId = child.internalId() & s_windowBitMask;
        return createIndex(parentId - 1, 0, parentId);
    }
    return QModelIndex();
}

void InputDeviceModel::slotPropertyChanged()
{
    const auto device = static_cast<InputDevice *>(sender());

    for (int i = 0; i < device->metaObject()->propertyCount(); ++i) {
        const QMetaProperty metaProperty = device->metaObject()->property(i);
        if (metaProperty.notifySignalIndex() == senderSignalIndex()) {
            const QModelIndex parent = index(m_devices.indexOf(device), 0, QModelIndex());
            const QModelIndex child = index(i, 1, parent);
            Q_EMIT dataChanged(child, child, QList<int>{Qt::DisplayRole});
        }
    }
}

void InputDeviceModel::setupDeviceConnections(InputDevice *device)
{
    QMetaMethod handler = metaObject()->method(metaObject()->indexOfMethod("slotPropertyChanged()"));
    for (int i = 0; i < device->metaObject()->propertyCount(); ++i) {
        const QMetaProperty metaProperty = device->metaObject()->property(i);
        if (metaProperty.hasNotifySignal()) {
            connect(device, metaProperty.notifySignal(), this, handler);
        }
    }
}

QModelIndex DataSourceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_source || parent.isValid() || column >= 2 || row >= m_source->mimeTypes().size()) {
        return QModelIndex();
    }
    return createIndex(row, column, nullptr);
}

QModelIndex DataSourceModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int DataSourceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_source ? m_source->mimeTypes().count() : 0;
    }
    return 0;
}

QVariant DataSourceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal || section >= 2) {
        return QVariant();
    }
    return section == 0 ? QStringLiteral("Mime type") : QStringLiteral("Content");
}

QVariant DataSourceModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::ParentIsInvalid | CheckIndexOption::IndexIsValid)) {
        return QVariant();
    }
    const QString mimeType = m_source->mimeTypes().at(index.row());
    ;
    if (index.column() == 0 && role == Qt::DisplayRole) {
        return mimeType;
    } else if (index.column() == 1 && index.row() < m_data.count()) {
        const QByteArray &data = m_data.at(index.row());
        if (mimeType.contains(QLatin1String("image"))) {
            if (role == Qt::DecorationRole) {
                return QImage::fromData(data);
            }
        } else if (role == Qt::DisplayRole) {
            return data;
        }
    }
    return QVariant();
}

static QByteArray readData(int fd)
{
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    FileDescriptor closeFd{fd};
    QByteArray data;
    while (true) {
        const int ready = poll(&pfd, 1, 1000);
        if (ready < 0) {
            if (errno != EINTR) {
                return QByteArrayLiteral("poll() failed: ") + strerror(errno);
            }
        } else if (ready == 0) {
            return QByteArrayLiteral("timeout reading from pipe");
        } else {
            char buf[4096];
            int n = read(fd, buf, sizeof buf);

            if (n < 0) {
                return QByteArrayLiteral("read failed: ") + strerror(errno);
            } else if (n == 0) {
                return data;
            } else if (n > 0) {
                data.append(buf, n);
            }
        }
    }
}

void DataSourceModel::setSource(AbstractDataSource *source)
{
    beginResetModel();
    m_source = source;
    m_data.clear();
    if (source) {
        const QStringList mimeTypes = m_source->mimeTypes();
        m_data.resize(mimeTypes.size());
        for (auto type = mimeTypes.begin(); type != mimeTypes.end(); ++type) {
            std::optional<Pipe> pipe = Pipe::create(O_CLOEXEC);
            if (!pipe) {
                continue;
            }
            source->requestData(*type, std::move(pipe->writeEndpoint));
            QFuture<QByteArray> data = QtConcurrent::run(readData, pipe->readEndpoint.take());
            auto watcher = new QFutureWatcher<QByteArray>(this);
            watcher->setFuture(data);
            const int index = type - mimeTypes.begin();
            connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, index, source = QPointer(source)] {
                watcher->deleteLater();
                if (source && source == m_source) {
                    m_data[index] = watcher->result();
                    Q_EMIT dataChanged(this->index(index, 1), this->index(index, 1), {Qt::DecorationRole | Qt::DisplayRole});
                }
            });
        }
    }
    endResetModel();
}

DebugConsoleEffectItem::DebugConsoleEffectItem(const QString &name, bool loaded, QWidget *parent)
    : QWidget(parent)
    , m_name(name)
    , m_loaded(loaded)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    m_label = new QLabel(name, this);
    layout->addWidget(m_label);

    m_toggleButton = new QPushButton(this);
    layout->addWidget(m_toggleButton);

    updateToggleButton();

    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        if (m_loaded) {
            m_loaded = false;
            effects->unloadEffect(m_name);
        } else {
            m_loaded = effects->loadEffect(m_name);
        }
        updateToggleButton();
    });
}

void DebugConsoleEffectItem::updateToggleButton()
{
    QFont font = m_label->font();
    if (m_loaded) {
        m_toggleButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackPause));
        m_toggleButton->setText(i18nc("@action:button unload an effect", "Unload"));
        font.setBold(true);
    } else {
        m_toggleButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart));
        m_toggleButton->setText(i18nc("@action:button load an effect", "Load"));
        font.setBold(false);
    }
    m_label->setFont(font);
}

DebugConsoleEffectsTab::DebugConsoleEffectsTab(QWidget *parent)
    : QListWidget(parent)
{
    if (!effects) {
        return;
    }

    QStringList availableEffects = effects->listOfEffects();
    const QStringList loadedEffects = effects->loadedEffects();

    // Remove any duplicates with the same name
    availableEffects.removeDuplicates();

    // Show these debugging effects at the top of the list, sort the rest so they can be easily found
    const QStringList priorityEffects = {QStringLiteral("showcompositing"), QStringLiteral("showfps"), QStringLiteral("showpaint")};
    std::sort(availableEffects.begin(), availableEffects.end(), [&priorityEffects](const QString &a, const QString &b) {
        const int indexA = priorityEffects.indexOf(a);
        const int indexB = priorityEffects.indexOf(b);

        if (indexA != -1 && indexB != -1) {
            return a < b;
        } else if (indexA != -1) {
            return true;
        } else if (indexB != -1) {
            return false;
        }

        return a < b;
    });

    // Determine the index of the last priority effect so we can insert a separator beneath
    int lastPriorityIndex = -1;
    for (int i = 0; i < availableEffects.count(); ++i) {
        if (priorityEffects.contains(availableEffects[i])) {
            lastPriorityIndex = i;
        } else {
            break;
        }
    }

    for (int i = 0; i < availableEffects.count(); ++i) {
        const QString &effectName = availableEffects[i];

        QListWidgetItem *item = new QListWidgetItem(this);
        DebugConsoleEffectItem *effectItem = new DebugConsoleEffectItem(effectName, loadedEffects.contains(effectName));

        addItem(item);
        setItemWidget(item, effectItem);
        item->setSizeHint(effectItem->sizeHint());

        if (i == lastPriorityIndex) {
            QListWidgetItem *separatorItem = new QListWidgetItem(this);
            separatorItem->setFlags(Qt::NoItemFlags);

            QFrame *separator = new QFrame();
            separator->setFrameShape(QFrame::HLine);

            addItem(separatorItem);
            setItemWidget(separatorItem, separator);
        }
    }
}

} // namespace KWin

#include "moc_debug_console.cpp"
