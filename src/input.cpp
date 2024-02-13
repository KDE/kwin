/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input.h"

#include "backends/fakeinput/fakeinputbackend.h"
#include "backends/libinput/connection.h"
#include "backends/libinput/device.h"
#include "core/inputbackend.h"
#include "core/session.h"
#include "effects.h"
#include "gestures.h"
#include "globalshortcuts.h"
#include "hide_cursor_spy.h"
#include "idledetector.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "inputmethod.h"
#include "keyboard_input.h"
#include "main.h"
#include "mousebuttons.h"
#include "pointer_input.h"
#include "tablet_input.h"
#include "touch_input.h"
#include "x11window.h"
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "cursorsource.h"
#include "internalwindow.h"
#include "popup_input_filter.h"
#include "screenedge.h"
#include "unmanaged.h"
#include "virtualdesktops.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/surface_interface.h"
#include "wayland/tablet_v2_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xkb.h"
#include "xwayland/xwayland_interface.h"

#include <KDecoration2/Decoration>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <decorations/decoratedclient.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif
// Qt
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeyEvent>
#include <QThread>
#include <qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon.h>

#include "osd.h"
#include <cmath>

using namespace std::literals;

namespace KWin
{

static KWaylandServer::PointerAxisSource kwinAxisSourceToKWaylandAxisSource(InputRedirection::PointerAxisSource source)
{
    switch (source) {
    case KWin::InputRedirection::PointerAxisSourceWheel:
        return KWaylandServer::PointerAxisSource::Wheel;
    case KWin::InputRedirection::PointerAxisSourceFinger:
        return KWaylandServer::PointerAxisSource::Finger;
    case KWin::InputRedirection::PointerAxisSourceContinuous:
        return KWaylandServer::PointerAxisSource::Continuous;
    case KWin::InputRedirection::PointerAxisSourceWheelTilt:
        return KWaylandServer::PointerAxisSource::WheelTilt;
    case KWin::InputRedirection::PointerAxisSourceUnknown:
    default:
        return KWaylandServer::PointerAxisSource::Unknown;
    }
}

InputEventFilter::InputEventFilter() = default;

InputEventFilter::~InputEventFilter()
{
    if (input()) {
        input()->uninstallInputEventFilter(this);
    }
}

bool InputEventFilter::pointerEvent(MouseEvent *event, quint32 nativeButton)
{
    return false;
}

bool InputEventFilter::wheelEvent(WheelEvent *event)
{
    return false;
}

bool InputEventFilter::keyEvent(KeyEvent *event)
{
    return false;
}

bool InputEventFilter::touchDown(qint32 id, const QPointF &point, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::touchMotion(qint32 id, const QPointF &point, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::touchUp(qint32 id, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::touchCancel()
{
    return false;
}

bool InputEventFilter::touchFrame()
{
    return false;
}

bool InputEventFilter::pinchGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::pinchGestureEnd(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::pinchGestureCancelled(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::swipeGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::swipeGestureEnd(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::swipeGestureCancelled(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::holdGestureBegin(int fingerCount, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::holdGestureEnd(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::holdGestureCancelled(std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::switchEvent(SwitchEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolEvent(TabletEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletId, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    return false;
}

bool InputEventFilter::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    return false;
}

void InputEventFilter::passToWaylandServer(QKeyEvent *event)
{
    Q_ASSERT(waylandServer());
    if (event->isAutoRepeat()) {
        return;
    }

    KWaylandServer::SeatInterface *seat = waylandServer()->seat();
    const int keyCode = event->nativeScanCode();
    switch (event->type()) {
    case QEvent::KeyPress:
        seat->notifyKeyboardKey(keyCode, KWaylandServer::KeyboardKeyState::Pressed);
        break;
    case QEvent::KeyRelease:
        seat->notifyKeyboardKey(keyCode, KWaylandServer::KeyboardKeyState::Released);
        break;
    default:
        break;
    }
}

bool InputEventFilter::passToInputMethod(QKeyEvent *event)
{
    if (!kwinApp()->inputMethod()) {
        return false;
    }
    if (auto keyboardGrab = kwinApp()->inputMethod()->keyboardGrab()) {
        if (event->isAutoRepeat()) {
            return true;
        }
        auto newState = event->type() == QEvent::KeyPress ? KWaylandServer::KeyboardKeyState::Pressed : KWaylandServer::KeyboardKeyState::Released;
        keyboardGrab->sendKey(waylandServer()->display()->nextSerial(), event->timestamp(), event->nativeScanCode(), newState);
        return true;
    } else {
        return false;
    }
}

class VirtualTerminalFilter : public InputEventFilter
{
public:
    bool keyEvent(KeyEvent *event) override
    {
        // really on press and not on release? X11 switches on press.
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            const xkb_keysym_t keysym = event->nativeVirtualKey();
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                kwinApp()->session()->switchTo(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
                return true;
            }
        }
        return false;
    }
};

class TerminateServerFilter : public InputEventFilter
{
public:
    bool keyEvent(KeyEvent *event) override
    {
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            if (event->nativeVirtualKey() == XKB_KEY_Terminate_Server) {
                qCWarning(KWIN_CORE) << "Request to terminate server";
                QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
                return true;
            }
        }
        return false;
    }
};

class LockScreenFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        auto window = input()->findToplevel(event->globalPos());
        if (window && window->isClient() && window->isLockScreen()) {
            workspace()->activateWindow(window);
        }

        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (event->type() == QEvent::MouseMove) {
            if (pointerSurfaceAllowed()) {
                // TODO: should the pointer position always stay in sync, i.e. not do the check?
                seat->notifyPointerMotion(event->screenPos());
                seat->notifyPointerFrame();
            }
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            if (pointerSurfaceAllowed()) {
                // TODO: can we leak presses/releases here when we move the mouse in between from an allowed surface to
                //       disallowed one or vice versa?
                const auto state = event->type() == QEvent::MouseButtonPress
                    ? KWaylandServer::PointerButtonState::Pressed
                    : KWaylandServer::PointerButtonState::Released;
                seat->notifyPointerButton(nativeButton, state);
                seat->notifyPointerFrame();
            }
        }
        return true;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            const WheelEvent *wheelEvent = static_cast<WheelEvent *>(event);
            seat->setTimestamp(wheelEvent->timestamp());
            seat->notifyPointerAxis(wheelEvent->orientation(), wheelEvent->delta(),
                                    wheelEvent->deltaV120(),
                                    kwinAxisSourceToKWaylandAxisSource(wheelEvent->axisSource()));
            seat->notifyPointerFrame();
        }
        return true;
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        if (event->isAutoRepeat()) {
            // wayland client takes care of it
            return true;
        }
        // send event to KSldApp for global accel
        // if event is set to accepted it means a whitelisted shortcut was triggered
        // in that case we filter it out and don't process it further
#if KWIN_BUILD_SCREENLOCKER
        event->setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), event);
        if (event->isAccepted()) {
            return true;
        }
#endif

        // continue normal processing
        input()->keyboard()->update();
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (!keyboardSurfaceAllowed()) {
            // don't pass event to seat
            return true;
        }
        switch (event->type()) {
        case QEvent::KeyPress:
            seat->notifyKeyboardKey(event->nativeScanCode(), KWaylandServer::KeyboardKeyState::Pressed);
            break;
        case QEvent::KeyRelease:
            seat->notifyKeyboardKey(event->nativeScanCode(), KWaylandServer::KeyboardKeyState::Released);
            break;
        default:
            break;
        }
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            seat->notifyTouchDown(id, pos);
        }
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            seat->notifyTouchMotion(id, pos);
        }
        return true;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            seat->notifyTouchUp(id);
        }
        return true;
    }
    bool pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureEnd(std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureCancelled(std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }

    bool swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureEnd(std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureCancelled(std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureEnd(std::chrono::microseconds time) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }

private:
    bool surfaceAllowed(KWaylandServer::SurfaceInterface *(KWaylandServer::SeatInterface::*method)() const) const
    {
        if (KWaylandServer::SurfaceInterface *s = (waylandServer()->seat()->*method)()) {
            if (Window *t = waylandServer()->findWindow(s)) {
                return t->isLockScreen() || t->isInputMethod() || t->isLockScreenOverlay();
            }
            return false;
        }
        return true;
    }
    bool pointerSurfaceAllowed() const
    {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedPointerSurface);
    }
    bool keyboardSurfaceAllowed() const
    {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedKeyboardSurface);
    }
    bool touchSurfaceAllowed() const
    {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedTouchSurface);
    }
};

class EffectsFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->checkInputWindowEvent(event);
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->checkInputWindowEvent(event);
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (!effects || !static_cast<EffectsHandlerImpl *>(effects)->hasKeyboardGrab()) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        passToWaylandServer(event);
        static_cast<EffectsHandlerImpl *>(effects)->grabbedKeyboardEvent(event);
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->touchDown(id, pos, time);
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->touchMotion(id, pos, time);
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->touchUp(id, time);
    }
    bool tabletToolEvent(TabletEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->tabletToolEvent(event);
    }
    bool tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->tabletToolButtonEvent(button, pressed, tabletToolId, time);
    }
    bool tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->tabletPadButtonEvent(button, pressed, tabletPadId, time);
    }
    bool tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->tabletPadStripEvent(number, position, isFinger, tabletPadId, time);
    }
    bool tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl *>(effects)->tabletPadRingEvent(number, position, isFinger, tabletPadId, time);
    }
};

class MoveResizeFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseMove:
            window->updateInteractiveMoveResize(event->screenPos());
            break;
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                window->endInteractiveMoveResize();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        // filter out while moving a window
        return workspace()->moveResizeWindow() != nullptr;
    }
    bool keyEvent(KeyEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            window->keyPressEvent(event->key() | event->modifiers());
            if (window->isInteractiveMove() || window->isInteractiveResize()) {
                // only update if mode didn't end
                window->updateInteractiveMoveResize(input()->globalPointer());
            }
        }
        return true;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        return true;
    }

    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (!m_set) {
            m_id = id;
            m_set = true;
        }
        if (m_id == id) {
            window->updateInteractiveMoveResize(pos);
        }
        return true;
    }

    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (m_id == id || !m_set) {
            window->endInteractiveMoveResize();
            m_set = false;
            // pass through to update decoration filter later on
            return false;
        }
        m_set = false;
        return true;
    }

    bool tabletToolEvent(TabletEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        switch (event->type()) {
        case QEvent::TabletMove:
            window->updateInteractiveMoveResize(event->globalPos());
            break;
        case QEvent::TabletRelease:
            window->endInteractiveMoveResize();
            break;
        default:
            break;
        }
        // Let TabletInputFilter receive the event, so the cursor position can be updated.
        return false;
    }

private:
    qint32 m_id = 0;
    bool m_set = false;
};

class WindowSelectorFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (!m_active) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                if (event->button() == Qt::RightButton) {
                    cancel();
                } else {
                    accept(event->globalPos());
                }
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        // filter out while selecting a window
        return m_active;
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (!m_active) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        passToWaylandServer(event);

        if (event->type() == QEvent::KeyPress) {
            // x11 variant does this on key press, so do the same
            if (event->key() == Qt::Key_Escape) {
                cancel();
            } else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) {
                accept(input()->globalPointer());
            }
            if (input()->supportsPointerWarping()) {
                int mx = 0;
                int my = 0;
                if (event->key() == Qt::Key_Left) {
                    mx = -10;
                }
                if (event->key() == Qt::Key_Right) {
                    mx = 10;
                }
                if (event->key() == Qt::Key_Up) {
                    my = -10;
                }
                if (event->key() == Qt::Key_Down) {
                    my = 10;
                }
                if (event->modifiers() & Qt::ControlModifier) {
                    mx /= 10;
                    my /= 10;
                }
                input()->warpPointer(input()->globalPointer() + QPointF(mx, my));
            }
        }
        // filter out while selecting a window
        return true;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!isActive()) {
            return false;
        }
        m_touchPoints.insert(id, pos);
        return true;
    }

    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (!isActive()) {
            return false;
        }
        auto it = m_touchPoints.find(id);
        if (it != m_touchPoints.end()) {
            *it = pos;
        }
        return true;
    }

    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        if (!isActive()) {
            return false;
        }
        auto it = m_touchPoints.find(id);
        if (it != m_touchPoints.end()) {
            const auto pos = it.value();
            m_touchPoints.erase(it);
            if (m_touchPoints.isEmpty()) {
                accept(pos);
            }
        }
        return true;
    }

    bool isActive() const
    {
        return m_active;
    }
    void start(std::function<void(KWin::Window *)> callback)
    {
        Q_ASSERT(!m_active);
        m_active = true;
        m_callback = callback;
        input()->keyboard()->update();
        input()->touch()->cancel();
    }
    void start(std::function<void(const QPoint &)> callback)
    {
        Q_ASSERT(!m_active);
        m_active = true;
        m_pointSelectionFallback = callback;
        input()->keyboard()->update();
        input()->touch()->cancel();
    }

private:
    void deactivate()
    {
        m_active = false;
        m_callback = std::function<void(KWin::Window *)>();
        m_pointSelectionFallback = std::function<void(const QPoint &)>();
        input()->pointer()->removeWindowSelectionCursor();
        input()->keyboard()->update();
        m_touchPoints.clear();
    }
    void cancel()
    {
        if (m_callback) {
            m_callback(nullptr);
        }
        if (m_pointSelectionFallback) {
            m_pointSelectionFallback(QPoint(-1, -1));
        }
        deactivate();
    }
    void accept(const QPointF &pos)
    {
        if (m_callback) {
            // TODO: this ignores shaped windows
            m_callback(input()->findToplevel(pos));
        }
        if (m_pointSelectionFallback) {
            m_pointSelectionFallback(pos.toPoint());
        }
        deactivate();
    }

    bool m_active = false;
    std::function<void(KWin::Window *)> m_callback;
    std::function<void(const QPoint &)> m_pointSelectionFallback;
    QMap<quint32, QPointF> m_touchPoints;
};

class GlobalShortcutFilter : public InputEventFilter
{
public:
    GlobalShortcutFilter()
    {
        m_powerDown.setSingleShot(true);
        m_powerDown.setInterval(1000);
    }

    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            if (input()->shortcuts()->processPointerPressed(event->modifiers(), event->buttons())) {
                return true;
            }
        }
        return false;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (event->modifiers() == Qt::NoModifier) {
            return false;
        }
        PointerAxisDirection direction = PointerAxisUp;
        if (event->angleDelta().x() < 0) {
            direction = PointerAxisRight;
        } else if (event->angleDelta().x() > 0) {
            direction = PointerAxisLeft;
        } else if (event->angleDelta().y() < 0) {
            direction = PointerAxisDown;
        } else if (event->angleDelta().y() > 0) {
            direction = PointerAxisUp;
        }
        return input()->shortcuts()->processAxis(event->modifiers(), direction);
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (event->key() == Qt::Key_PowerOff) {
            const auto modifiers = static_cast<KeyEvent *>(event)->modifiersRelevantForGlobalShortcuts();
            if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
                QObject::connect(&m_powerDown, &QTimer::timeout, input()->shortcuts(), [this, modifiers] {
                    QObject::disconnect(&m_powerDown, &QTimer::timeout, input()->shortcuts(), nullptr);
                    m_powerDown.stop();
                    input()->shortcuts()->processKey(modifiers, Qt::Key_PowerDown);
                });
                m_powerDown.start();
                return true;
            } else if (event->type() == QEvent::KeyRelease) {
                const bool ret = !m_powerDown.isActive() || input()->shortcuts()->processKey(modifiers, event->key());
                m_powerDown.stop();
                return ret;
            }
        } else if (event->type() == QEvent::KeyPress) {
            if (!waylandServer()->isKeyboardShortcutsInhibited()) {
                return input()->shortcuts()->processKey(static_cast<KeyEvent *>(event)->modifiersRelevantForGlobalShortcuts(), event->key());
            }
        } else if (event->type() == QEvent::KeyRelease) {
            if (!waylandServer()->isKeyboardShortcutsInhibited()) {
                return input()->shortcuts()->processKeyRelease(static_cast<KeyEvent *>(event)->modifiersRelevantForGlobalShortcuts(), event->key());
            }
        }
        return false;
    }
    bool swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        m_touchpadGestureFingerCount = fingerCount;
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeStart(DeviceType::Touchpad, fingerCount);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeUpdate(DeviceType::Touchpad, delta);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureCancelled(std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeCancel(DeviceType::Touchpad);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureEnd(std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeEnd(DeviceType::Touchpad);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        m_touchpadGestureFingerCount = fingerCount;
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchStart(fingerCount);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchUpdate(scale, angleDelta, delta);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureEnd(std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchEnd();
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureCancelled(std::chrono::microseconds time) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchCancel();
            return true;
        } else {
            return false;
        }
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (m_gestureTaken) {
            input()->shortcuts()->processSwipeCancel(DeviceType::Touchscreen);
            m_gestureCancelled = true;
            return true;
        } else {
            m_touchPoints.insert(id, pos);
            if (m_touchPoints.count() == 1) {
                m_lastTouchDownTime = time;
            } else {
                if (time - m_lastTouchDownTime > 250ms) {
                    m_gestureCancelled = true;
                    return false;
                }
                m_lastTouchDownTime = time;
                auto output = workspace()->outputAt(pos);
                auto physicalSize = output->orientateSize(output->physicalSize());
                if (!physicalSize.isValid()) {
                    physicalSize = QSize(190, 100);
                }
                float xfactor = physicalSize.width() / (float)output->geometry().width();
                float yfactor = physicalSize.height() / (float)output->geometry().height();
                bool distanceMatch = std::any_of(m_touchPoints.constBegin(), m_touchPoints.constEnd(), [pos, xfactor, yfactor](const auto &point) {
                    QPointF p = pos - point;
                    return std::abs(xfactor * p.x()) + std::abs(yfactor * p.y()) < 50;
                });
                if (!distanceMatch) {
                    m_gestureCancelled = true;
                    return false;
                }
            }
            if (m_touchPoints.count() >= 3 && !m_gestureCancelled) {
                m_gestureTaken = true;
                m_syntheticCancel = true;
                input()->processFilters(std::bind(&InputEventFilter::touchCancel, std::placeholders::_1));
                m_syntheticCancel = false;
                input()->shortcuts()->processSwipeStart(DeviceType::Touchscreen, m_touchPoints.count());
                return true;
            }
        }
        return false;
    }

    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (m_gestureTaken) {
            if (m_gestureCancelled) {
                return true;
            }
            auto output = workspace()->outputAt(pos);
            const auto physicalSize = output->orientateSize(output->physicalSize());
            const float xfactor = physicalSize.width() / (float)output->geometry().width();
            const float yfactor = physicalSize.height() / (float)output->geometry().height();

            auto &point = m_touchPoints[id];
            const QPointF dist = pos - point;
            const QPointF delta = QPointF(xfactor * dist.x(), yfactor * dist.y());
            input()->shortcuts()->processSwipeUpdate(DeviceType::Touchscreen, 5 * delta / m_touchPoints.size());
            point = pos;
            return true;
        }
        return false;
    }

    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        m_touchPoints.remove(id);
        if (m_gestureTaken) {
            if (!m_gestureCancelled) {
                input()->shortcuts()->processSwipeEnd(DeviceType::Touchscreen);
                m_gestureCancelled = true;
            }
            m_gestureTaken &= m_touchPoints.count() > 0;
            m_gestureCancelled &= m_gestureTaken;
            return true;
        } else {
            m_gestureCancelled &= m_touchPoints.count() > 0;
            return false;
        }
    }

    bool touchCancel() override
    {
        if (m_syntheticCancel) {
            return false;
        }
        const bool oldGestureTaken = m_gestureTaken;
        m_gestureTaken = false;
        m_gestureCancelled = false;
        m_touchPoints.clear();
        return oldGestureTaken;
    }

    bool touchFrame() override
    {
        return m_gestureTaken;
    }

private:
    bool m_gestureTaken = false;
    bool m_gestureCancelled = false;
    bool m_syntheticCancel = false;
    std::chrono::microseconds m_lastTouchDownTime = std::chrono::microseconds::zero();
    QPointF m_lastAverageDistance;
    QMap<int32_t, QPointF> m_touchPoints;
    int m_touchpadGestureFingerCount = 0;

    QTimer m_powerDown;
};

namespace
{

enum class MouseAction {
    ModifierOnly,
    ModifierAndWindow
};
std::pair<bool, bool> performWindowMouseAction(QMouseEvent *event, Window *window, MouseAction action = MouseAction::ModifierOnly)
{
    Options::MouseCommand command = Options::MouseNothing;
    bool wasAction = false;
    if (static_cast<MouseEvent *>(event)->modifiersRelevantForGlobalShortcuts() == options->commandAllModifier()) {
        if (!input()->pointer()->isConstrained() && !workspace()->globalShortcutsDisabled()) {
            wasAction = true;
            switch (event->button()) {
            case Qt::LeftButton:
                command = options->commandAll1();
                break;
            case Qt::MiddleButton:
                command = options->commandAll2();
                break;
            case Qt::RightButton:
                command = options->commandAll3();
                break;
            default:
                // nothing
                break;
            }
        }
    } else {
        if (action == MouseAction::ModifierAndWindow) {
            command = window->getMouseCommand(event->button(), &wasAction);
        }
    }
    if (wasAction) {
        return std::make_pair(wasAction, !window->performMouseCommand(command, event->globalPos()));
    }
    return std::make_pair(wasAction, false);
}

std::pair<bool, bool> performWindowWheelAction(QWheelEvent *event, Window *window, MouseAction action = MouseAction::ModifierOnly)
{
    bool wasAction = false;
    Options::MouseCommand command = Options::MouseNothing;
    if (static_cast<WheelEvent *>(event)->modifiersRelevantForGlobalShortcuts() == options->commandAllModifier()) {
        if (!input()->pointer()->isConstrained() && !workspace()->globalShortcutsDisabled()) {
            wasAction = true;
            command = options->operationWindowMouseWheel(-1 * event->angleDelta().y());
        }
    } else {
        if (action == MouseAction::ModifierAndWindow) {
            command = window->getWheelCommand(Qt::Vertical, &wasAction);
        }
    }
    if (wasAction) {
        return std::make_pair(wasAction, !window->performMouseCommand(command, event->globalPosition()));
    }
    return std::make_pair(wasAction, false);
}

}

class InternalWindowEventFilter : public InputEventFilter
{
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->pointer()->focus())->handle();
        if (!internal) {
            // the handle can be nullptr if the tooltip gets closed while focus updates are blocked
            return false;
        }
        QMouseEvent mouseEvent(event->type(),
                               event->pos() - internal->position(),
                               event->globalPos(),
                               event->button(), event->buttons(), event->modifiers());
        QCoreApplication::sendEvent(internal, &mouseEvent);
        return mouseEvent.isAccepted();
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->pointer()->focus())->handle();
        const QPointF localPos = event->globalPosition() - internal->position();
        QWheelEvent wheelEvent(localPos, event->globalPosition(), QPoint(),
                               event->angleDelta() * -1,
                               event->buttons(),
                               event->modifiers(),
                               Qt::NoScrollPhase,
                               false);
        QCoreApplication::sendEvent(internal, &wheelEvent);
        return wheelEvent.isAccepted();
    }
    bool keyEvent(KeyEvent *event) override
    {
        const QList<InternalWindow *> &windows = workspace()->internalWindows();
        QWindow *found = nullptr;
        for (auto it = windows.crbegin(); it != windows.crend(); ++it) {
            if (QWindow *w = (*it)->handle()) {
                if (!w->isVisible()) {
                    continue;
                }
                if (!workspace()->geometry().contains(w->geometry())) {
                    continue;
                }
                if (w->property("_q_showWithoutActivating").toBool()) {
                    continue;
                }
                if (w->property("outputOnly").toBool()) {
                    continue;
                }
                if (w->flags().testFlag(Qt::ToolTip)) {
                    continue;
                }
                found = w;
                break;
            }
        }
        if (QGuiApplication::focusWindow() != found) {
            QWindowSystemInterface::handleWindowActivated(found);
        }
        if (!found) {
            return false;
        }
        auto xkb = input()->keyboard()->xkb();
        Qt::Key key = xkb->toQtKey(xkb->toKeysym(event->nativeScanCode()),
                                   event->nativeScanCode(),
                                   Qt::KeyboardModifiers(),
                                   true /* workaround for QTBUG-62102 */);
        QKeyEvent internalEvent(event->type(), key,
                                event->modifiers(), event->nativeScanCode(), event->nativeVirtualKey(),
                                event->nativeModifiers(), event->text());
        internalEvent.setAccepted(false);
        if (QCoreApplication::sendEvent(found, &internalEvent)) {
            waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
            passToWaylandServer(event);
            return true;
        }
        return false;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            // something else is getting the events
            return false;
        }
        auto touch = input()->touch();
        if (touch->internalPressId() != -1) {
            // already on internal window, ignore further touch points, but filter out
            m_pressedIds.insert(id);
            return true;
        }
        // a new touch point
        seat->setTimestamp(time);
        if (!input()->touch()->focus() || !input()->touch()->focus()->isInternal()) {
            return false;
        }
        touch->setInternalPressId(id);
        // Qt's touch event API is rather complex, let's do fake mouse events instead
        QWindow *internal = static_cast<InternalWindow *>(input()->touch()->focus())->handle();
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - internal->position();

        QEnterEvent enterEvent(m_lastLocalTouchPos, m_lastLocalTouchPos, pos);
        QCoreApplication::sendEvent(internal, &enterEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal, &e);
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto touch = input()->touch();
        if (!input()->touch()->focus() || !input()->touch()->focus()->isInternal()) {
            return false;
        }
        if (touch->internalPressId() == -1) {
            return false;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id) || m_pressedIds.contains(id)) {
            // ignore, but filter out
            return true;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->touch()->focus())->handle();
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());

        QMouseEvent e(QEvent::MouseMove, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        QCoreApplication::instance()->sendEvent(internal, &e);
        return true;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        auto touch = input()->touch();
        const bool removed = m_pressedIds.remove(id);
        if (touch->internalPressId() == -1) {
            return removed;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        if (!input()->touch()->focus() || !input()->touch()->focus()->isInternal()) {
            return removed;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->touch()->focus())->handle();
        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal, &e);

        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(internal, &leaveEvent);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setInternalPressId(-1);
        return true;
    }

private:
    QSet<qint32> m_pressedIds;
    QPointF m_lastGlobalTouchPos;
    QPointF m_lastLocalTouchPos;
};

class MouseWheelAccumulator
{
public:
    float accumulate(WheelEvent *event)
    {
        m_scrollV120 += event->deltaV120();
        m_scrollDistance += event->delta();
        if (std::abs(m_scrollV120) >= 120 || (!event->deltaV120() && std::abs(m_scrollDistance) >= 15)) {
            float ret = m_scrollDistance;
            m_scrollV120 = 0;
            m_scrollDistance = 0;
            return ret;
        } else {
            return 0;
        }
    }

private:
    float m_scrollDistance = 0;
    int m_scrollV120 = 0;
};

class DecorationEventFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->screenPos() - decoration->window()->pos();
        switch (event->type()) {
        case QEvent::MouseMove: {
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->window()->processDecorationMove(p, event->screenPos());
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            const auto actionResult = performWindowMouseAction(event, decoration->window());
            if (actionResult.first) {
                return actionResult.second;
            }
            QMouseEvent e(event->type(), p, event->screenPos(), event->button(), event->buttons(), event->modifiers());
            e.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp()).count());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && event->type() == QEvent::MouseButtonPress) {
                decoration->window()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                decoration->window()->processDecorationButtonRelease(&e);
            }
            return true;
        }
        default:
            break;
        }
        return false;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        if (event->angleDelta().y() != 0) {
            // client window action only on vertical scrolling
            const auto actionResult = performWindowWheelAction(event, decoration->window());
            if (actionResult.first) {
                return actionResult.second;
            }
        }
        const QPointF localPos = event->globalPosition() - decoration->window()->pos();
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        QWheelEvent e(localPos, event->globalPosition(), QPoint(),
                      event->angleDelta(),
                      event->buttons(),
                      event->modifiers(),
                      Qt::NoScrollPhase,
                      false);
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration, &e);
        if (e.isAccepted()) {
            return true;
        }
        if ((orientation == Qt::Vertical) && decoration->window()->titlebarPositionUnderMouse()) {
            if (float delta = m_accumulator.accumulate(event)) {
                decoration->window()->performMouseCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                          event->globalPosition());
            }
        }
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        if (input()->touch()->decorationPressId() != -1) {
            // already on a decoration, ignore further touch points, but filter out
            return true;
        }
        seat->setTimestamp(time);
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }

        input()->touch()->setDecorationPressId(id);
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - decoration->window()->pos();

        QHoverEvent hoverEvent(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::sendEvent(decoration->decoration(), &hoverEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (!e.isAccepted()) {
            decoration->window()->processDecorationButtonPress(&e);
        }
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }
        if (input()->touch()->decorationPressId() == -1) {
            return false;
        }
        if (input()->touch()->decorationPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - decoration->window()->pos();

        QHoverEvent e(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationMove(m_lastLocalTouchPos, pos);
        return true;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            // can happen when quick tiling
            if (input()->touch()->decorationPressId() == id) {
                m_lastGlobalTouchPos = QPointF();
                m_lastLocalTouchPos = QPointF();
                input()->touch()->setDecorationPressId(-1);
                return true;
            }
            return false;
        }
        if (input()->touch()->decorationPressId() == -1) {
            return false;
        }
        if (input()->touch()->decorationPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }

        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationButtonRelease(&e);

        QHoverEvent leaveEvent(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::sendEvent(decoration->decoration(), &leaveEvent);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setDecorationPressId(-1);
        return true;
    }
    bool tabletToolEvent(TabletEvent *event) override
    {
        auto decoration = input()->tablet()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->globalPosF() - decoration->window()->pos();
        switch (event->type()) {
        case QEvent::TabletMove:
        case QEvent::TabletEnterProximity: {
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->window()->processDecorationMove(p, event->globalPosF());
            break;
        }
        case QEvent::TabletPress:
        case QEvent::TabletRelease: {
            const bool isPressed = event->type() == QEvent::TabletPress;
            QMouseEvent e(isPressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                          p,
                          event->globalPosF(),
                          Qt::LeftButton,
                          isPressed ? Qt::LeftButton : Qt::MouseButtons(),
                          input()->keyboardModifiers());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && isPressed) {
                decoration->window()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::TabletRelease) {
                decoration->window()->processDecorationButtonRelease(&e);
            }
            break;
        }
        case QEvent::TabletLeaveProximity: {
            QHoverEvent leaveEvent(QEvent::HoverLeave, QPointF(), QPointF());
            QCoreApplication::sendEvent(decoration->decoration(), &leaveEvent);
            break;
        }
        default:
            break;
        }
        // Let TabletInputFilter receive the event, so the tablet can be registered and the cursor position can be updated.
        return false;
    }

private:
    QPointF m_lastGlobalTouchPos;
    QPointF m_lastLocalTouchPos;
    MouseWheelAccumulator m_accumulator;
};

#if KWIN_BUILD_TABBOX
class TabBoxInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 button) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        return workspace()->tabbox()->handleMouseEvent(event);
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setFocusedKeyboardSurface(nullptr);
        input()->pointer()->setEnableConstraints(false);
        // pass the key event to the seat, so that it has a proper model of the currently hold keys
        // this is important for combinations like alt+shift to ensure that shift is not considered pressed
        passToWaylandServer(event);

        if (event->type() == QEvent::KeyPress) {
            workspace()->tabbox()->keyPress(event->modifiers() | event->key());
        } else if (static_cast<KeyEvent *>(event)->modifiersRelevantForGlobalShortcuts() == Qt::NoModifier) {
            workspace()->tabbox()->modifiersReleased();
        }
        return true;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        return workspace()->tabbox()->handleWheelEvent(event);
    }
};
#endif

class ScreenEdgeInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        workspace()->screenEdges()->isEntered(event);
        // always forward
        return false;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        // TODO: better check whether a touch sequence is in progress
        if (m_touchInProgress || waylandServer()->seat()->isTouchSequence()) {
            // cancel existing touch
            workspace()->screenEdges()->gestureRecognizer()->cancelSwipeGesture();
            m_touchInProgress = false;
            m_id = 0;
            return false;
        }
        if (workspace()->screenEdges()->gestureRecognizer()->startSwipeGesture(pos) > 0) {
            m_touchInProgress = true;
            m_id = id;
            m_lastPos = pos;
            return true;
        }
        return false;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        if (m_touchInProgress && m_id == id) {
            workspace()->screenEdges()->gestureRecognizer()->updateSwipeGesture(pos - m_lastPos);
            m_lastPos = pos;
            return true;
        }
        return false;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        if (m_touchInProgress && m_id == id) {
            workspace()->screenEdges()->gestureRecognizer()->endSwipeGesture();
            m_touchInProgress = false;
            return true;
        }
        return false;
    }

private:
    bool m_touchInProgress = false;
    qint32 m_id = 0;
    QPointF m_lastPos;
};

/**
 * This filter implements window actions. If the event should not be passed to the
 * current pointer window it will filter out the event
 */
class WindowActionInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        if (event->type() != QEvent::MouseButtonPress) {
            return false;
        }
        Window *window = input()->pointer()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        const auto actionResult = performWindowMouseAction(event, window, MouseAction::ModifierAndWindow);
        if (actionResult.first) {
            return actionResult.second;
        }
        return false;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        if (event->angleDelta().y() == 0) {
            // only actions on vertical scroll
            return false;
        }
        Window *window = input()->pointer()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        const auto actionResult = performWindowWheelAction(event, window, MouseAction::ModifierAndWindow);
        if (actionResult.first) {
            return actionResult.second;
        }
        return false;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        Window *window = input()->touch()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = window->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !window->performMouseCommand(command, pos);
        }
        return false;
    }
    bool tabletToolEvent(TabletEvent *event) override
    {
        if (event->type() != QEvent::TabletPress) {
            return false;
        }
        Window *window = input()->tablet()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = window->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !window->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
};

class InputKeyboardFilter : public InputEventFilter
{
public:
    bool keyEvent(KeyEvent *event) override
    {
        return passToInputMethod(event);
    }
};

/**
 * The remaining default input filter which forwards events to other windows
 */
class ForwardInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove: {
            seat->notifyPointerMotion(event->globalPos());
            MouseEvent *e = static_cast<MouseEvent *>(event);
            if (!e->delta().isNull()) {
                seat->relativePointerMotion(e->delta(), e->deltaUnaccelerated(), e->timestamp());
            }
            seat->notifyPointerFrame();
            break;
        }
        case QEvent::MouseButtonPress:
            seat->notifyPointerButton(nativeButton, KWaylandServer::PointerButtonState::Pressed);
            seat->notifyPointerFrame();
            break;
        case QEvent::MouseButtonRelease:
            seat->notifyPointerButton(nativeButton, KWaylandServer::PointerButtonState::Released);
            seat->notifyPointerFrame();
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(WheelEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        auto _event = static_cast<WheelEvent *>(event);
        seat->notifyPointerAxis(_event->orientation(), _event->delta(), _event->deltaV120(),
                                kwinAxisSourceToKWaylandAxisSource(_event->axisSource()));
        seat->notifyPointerFrame();
        return true;
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat()) {
            // handled by Wayland client
            return false;
        }
        auto seat = waylandServer()->seat();
        input()->keyboard()->update();
        seat->setTimestamp(event->timestamp());
        passToWaylandServer(event);
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchDown(id, pos);
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchMotion(id, pos);
        return true;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchUp(id);
        return true;
    }
    bool touchCancel() override
    {
        waylandServer()->seat()->notifyTouchCancel();
        return true;
    }
    bool touchFrame() override
    {
        waylandServer()->seat()->notifyTouchFrame();
        return true;
    }
    bool pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerPinchGesture(fingerCount);
        return true;
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerPinchGesture(delta, scale, angleDelta);
        return true;
    }
    bool pinchGestureEnd(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerPinchGesture();
        return true;
    }
    bool pinchGestureCancelled(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerPinchGesture();
        return true;
    }

    bool swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerSwipeGesture(fingerCount);
        return true;
    }
    bool swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerSwipeGesture(delta);
        return true;
    }
    bool swipeGestureEnd(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerSwipeGesture();
        return true;
    }
    bool swipeGestureCancelled(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerSwipeGesture();
        return true;
    }
    bool holdGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerHoldGesture(fingerCount);
        return true;
    }
    bool holdGestureEnd(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerHoldGesture();
        return true;
    }
    bool holdGestureCancelled(std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerHoldGesture();
        return true;
    }
};

static KWaylandServer::SeatInterface *findSeat()
{
    auto server = waylandServer();
    if (!server) {
        return nullptr;
    }
    return server->seat();
}

class SurfaceCursor : public Cursor
{
public:
    explicit SurfaceCursor(KWaylandServer::TabletToolV2Interface *tool)
        : Cursor(tool)
        , m_source(std::make_unique<ImageCursorSource>())
    {
        setSource(m_source.get());
        connect(tool, &KWaylandServer::TabletToolV2Interface::cursorChanged, this, [this](KWaylandServer::TabletCursorV2 *tcursor) {
            if (!tcursor || tcursor->enteredSerial() == 0) {
                static WaylandCursorImage defaultCursor;
                defaultCursor.loadThemeCursor(CursorShape(Qt::CrossCursor), m_source.get());
                return;
            }
            auto cursorSurface = tcursor->surface();
            if (!cursorSurface) {
                m_source->update(QImage(), QPoint());
                return;
            }

            updateCursorSurface(cursorSurface, tcursor->hotspot());
        });
    }

    void updateCursorSurface(KWaylandServer::SurfaceInterface *surface, const QPoint &hotspot)
    {
        if (m_surface == surface && hotspot == m_hotspot) {
            return;
        }

        if (m_surface) {
            disconnect(m_surface, nullptr, this, nullptr);
        }
        m_surface = surface;
        m_hotspot = hotspot;
        connect(m_surface, &KWaylandServer::SurfaceInterface::committed, this, &SurfaceCursor::refresh);

        refresh();
    }

private:
    void refresh()
    {
        auto buffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(m_surface->buffer());
        if (!buffer) {
            m_source->update(QImage(), QPoint());
            return;
        }

        QImage cursorImage;
        cursorImage = buffer->data().copy();
        cursorImage.setDevicePixelRatio(m_surface->bufferScale());
        m_source->update(cursorImage, m_hotspot);
    }

    QPointer<KWaylandServer::SurfaceInterface> m_surface;
    std::unique_ptr<ImageCursorSource> m_source;
    QPoint m_hotspot;
};

/**
 * Handles input coming from a tablet device (e.g. wacom) often with a pen
 */
class TabletInputFilter : public QObject, public InputEventFilter
{
public:
    TabletInputFilter()
    {
        const auto devices = input()->devices();
        for (InputDevice *device : devices) {
            integrateDevice(device);
        }
        connect(input(), &InputRedirection::deviceAdded, this, &TabletInputFilter::integrateDevice);
        connect(input(), &InputRedirection::deviceRemoved, this, &TabletInputFilter::removeDevice);

        auto tabletNextOutput = new QAction(this);
        tabletNextOutput->setProperty("componentName", QStringLiteral("kwin"));
        tabletNextOutput->setText(i18n("Move the tablet to the next output"));
        tabletNextOutput->setObjectName(QStringLiteral("Move Tablet to Next Output"));
        KGlobalAccel::setGlobalShortcut(tabletNextOutput, QList<QKeySequence>());
        connect(tabletNextOutput, &QAction::triggered, this, &TabletInputFilter::trackNextOutput);
    }

    static KWaylandServer::TabletSeatV2Interface *findTabletSeat()
    {
        auto server = waylandServer();
        if (!server) {
            return nullptr;
        }
        KWaylandServer::TabletManagerV2Interface *manager = server->tabletManagerV2();
        return manager->seat(findSeat());
    }

    void integrateDevice(InputDevice *inputDevice)
    {
        auto device = qobject_cast<LibInput::Device *>(inputDevice);
        if (!device || (!device->isTabletTool() && !device->isTabletPad())) {
            return;
        }

        KWaylandServer::TabletSeatV2Interface *tabletSeat = findTabletSeat();
        if (!tabletSeat) {
            qCCritical(KWIN_CORE) << "Could not find tablet seat";
            return;
        }
        struct udev_device *const udev_device = libinput_device_get_udev_device(device->device());
        const char *devnode = udev_device_get_syspath(udev_device);

        auto deviceGroup = libinput_device_get_device_group(device->device());
        auto tablet = static_cast<KWaylandServer::TabletV2Interface *>(libinput_device_group_get_user_data(deviceGroup));
        if (!tablet) {
            tablet = tabletSeat->addTablet(device->vendor(), device->product(), device->sysName(), device->name(), {QString::fromUtf8(devnode)});
            libinput_device_group_set_user_data(deviceGroup, tablet);
        }

        if (device->isTabletPad()) {
            const int buttonsCount = libinput_device_tablet_pad_get_num_buttons(device->device());
            const int ringsCount = libinput_device_tablet_pad_get_num_rings(device->device());
            const int stripsCount = libinput_device_tablet_pad_get_num_strips(device->device());
            const int modes = libinput_device_tablet_pad_get_num_mode_groups(device->device());

            auto firstGroup = libinput_device_tablet_pad_get_mode_group(device->device(), 0);
            tabletSeat->addTabletPad(device->sysName(), device->name(), {QString::fromUtf8(devnode)}, buttonsCount, ringsCount, stripsCount, modes, libinput_tablet_pad_mode_group_get_mode(firstGroup), tablet);
        }
    }

    static void trackNextOutput()
    {
        const auto outputs = workspace()->outputs();
        if (outputs.isEmpty()) {
            return;
        }

        int tabletToolCount = 0;
        InputDevice *changedDevice = nullptr;
        const auto devices = input()->devices();
        for (const auto device : devices) {
            if (!device->isTabletTool()) {
                continue;
            }

            tabletToolCount++;
            if (device->outputName().isEmpty()) {
                device->setOutputName(outputs.constFirst()->name());
                changedDevice = device;
                continue;
            }

            auto it = std::find_if(outputs.begin(), outputs.end(), [device](const auto &output) {
                return output->name() == device->outputName();
            });
            ++it;
            auto nextOutput = it == outputs.end() ? outputs.first() : *it;
            device->setOutputName(nextOutput->name());
            changedDevice = device;
        }
        const QString message = tabletToolCount == 1 ? i18n("Tablet moved to %1", changedDevice->outputName()) : i18n("Tablets switched outputs");
        OSD::show(message, QStringLiteral("input-tablet"), 5000);
    }

    void removeDevice(InputDevice *inputDevice)
    {
        auto device = qobject_cast<LibInput::Device *>(inputDevice);
        if (device) {
            auto deviceGroup = libinput_device_get_device_group(device->device());
            libinput_device_group_set_user_data(deviceGroup, nullptr);

            KWaylandServer::TabletSeatV2Interface *tabletSeat = findTabletSeat();
            if (tabletSeat) {
                tabletSeat->removeDevice(device->sysName());
            } else {
                qCCritical(KWIN_CORE) << "Could not find tablet to remove" << device->sysName();
            }
        }
    }

    KWaylandServer::TabletToolV2Interface::Type getType(const KWin::TabletToolId &tabletToolId)
    {
        using Type = KWaylandServer::TabletToolV2Interface::Type;
        switch (tabletToolId.m_toolType) {
        case InputRedirection::Pen:
            return Type::Pen;
        case InputRedirection::Eraser:
            return Type::Eraser;
        case InputRedirection::Brush:
            return Type::Brush;
        case InputRedirection::Pencil:
            return Type::Pencil;
        case InputRedirection::Airbrush:
            return Type::Airbrush;
        case InputRedirection::Finger:
            return Type::Finger;
        case InputRedirection::Mouse:
            return Type::Mouse;
        case InputRedirection::Lens:
            return Type::Lens;
        case InputRedirection::Totem:
            return Type::Totem;
        }
        return Type::Pen;
    }

    KWaylandServer::TabletToolV2Interface *createTool(const KWin::TabletToolId &tabletToolId)
    {
        using namespace KWaylandServer;
        KWaylandServer::TabletSeatV2Interface *tabletSeat = findTabletSeat();

        const auto f = [](InputRedirection::Capability cap) {
            switch (cap) {
            case InputRedirection::Tilt:
                return TabletToolV2Interface::Tilt;
            case InputRedirection::Pressure:
                return TabletToolV2Interface::Pressure;
            case InputRedirection::Distance:
                return TabletToolV2Interface::Distance;
            case InputRedirection::Rotation:
                return TabletToolV2Interface::Rotation;
            case InputRedirection::Slider:
                return TabletToolV2Interface::Slider;
            case InputRedirection::Wheel:
                return TabletToolV2Interface::Wheel;
            }
            return TabletToolV2Interface::Wheel;
        };
        QVector<TabletToolV2Interface::Capability> ifaceCapabilities;
        ifaceCapabilities.resize(tabletToolId.m_capabilities.size());
        std::transform(tabletToolId.m_capabilities.constBegin(), tabletToolId.m_capabilities.constEnd(), ifaceCapabilities.begin(), f);

        TabletToolV2Interface *tool = tabletSeat->addTool(getType(tabletToolId), tabletToolId.m_serialId, tabletToolId.m_uniqueId, ifaceCapabilities, tabletToolId.deviceSysName);

        const auto cursor = new SurfaceCursor(tool);
        Cursors::self()->addCursor(cursor);
        m_cursorByTool[tool] = cursor;

        return tool;
    }

    bool tabletToolEvent(TabletEvent *event) override
    {
        using namespace KWaylandServer;

        if (!workspace()) {
            return false;
        }

        KWaylandServer::TabletSeatV2Interface *tabletSeat = findTabletSeat();
        if (!tabletSeat) {
            qCCritical(KWIN_CORE) << "Could not find tablet manager";
            return false;
        }
        auto tool = tabletSeat->toolByHardwareSerial(event->tabletId().m_serialId, getType(event->tabletId()));
        if (!tool) {
            tool = createTool(event->tabletId());
        }

        // NOTE: tablet will be nullptr as the device is removed (see ::removeDevice) but events from the tool
        // may still happen (e.g. Release or ProximityOut events)
        auto tablet = static_cast<KWaylandServer::TabletV2Interface *>(event->tabletId().m_deviceGroupData);

        Window *window = input()->findToplevel(event->globalPos());
        if (!window || !window->surface()) {
            return false;
        }

        KWaylandServer::SurfaceInterface *surface = window->surface();
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || (tablet && !tablet->isSurfaceSupported(surface))) {
            return emulateTabletEvent(event);
        }

        switch (event->type()) {
        case QEvent::TabletMove: {
            const auto pos = window->mapToLocal(event->globalPosF());
            tool->sendMotion(pos);
            m_cursorByTool[tool]->setPos(event->globalPos());
            break;
        }
        case QEvent::TabletEnterProximity: {
            const QPoint pos = event->globalPos();
            m_cursorByTool[tool]->setPos(pos);
            tool->sendProximityIn(tablet);
            tool->sendMotion(window->mapToLocal(event->globalPosF()));
            break;
        }
        case QEvent::TabletLeaveProximity:
            tool->sendProximityOut();
            break;
        case QEvent::TabletPress: {
            const auto pos = window->mapToLocal(event->globalPosF());
            tool->sendMotion(pos);
            m_cursorByTool[tool]->setPos(event->globalPos());
            tool->sendDown();
            break;
        }
        case QEvent::TabletRelease:
            tool->sendUp();
            break;
        default:
            qCWarning(KWIN_CORE) << "Unexpected tablet event type" << event;
            break;
        }
        const quint32 MAX_VAL = 65535;

        if (tool->hasCapability(TabletToolV2Interface::Pressure)) {
            tool->sendPressure(MAX_VAL * event->pressure());
        }
        if (tool->hasCapability(TabletToolV2Interface::Tilt)) {
            tool->sendTilt(event->xTilt(), event->yTilt());
        }
        if (tool->hasCapability(TabletToolV2Interface::Rotation)) {
            tool->sendRotation(event->rotation());
        }

        tool->sendFrame(event->timestamp());
        return true;
    }

    bool emulateTabletEvent(TabletEvent *event)
    {
        if (!workspace()) {
            return false;
        }

        switch (event->type()) {
        case QEvent::TabletMove:
        case QEvent::TabletEnterProximity:
            input()->pointer()->processMotionAbsolute(event->globalPosF(), std::chrono::milliseconds(event->timestamp()));
            break;
        case QEvent::TabletPress:
            input()->pointer()->processButton(KWin::qtMouseButtonToButton(Qt::LeftButton),
                                              InputRedirection::PointerButtonPressed, std::chrono::milliseconds(event->timestamp()));
            break;
        case QEvent::TabletRelease:
            input()->pointer()->processButton(KWin::qtMouseButtonToButton(Qt::LeftButton),
                                              InputRedirection::PointerButtonReleased, std::chrono::milliseconds(event->timestamp()));
            break;
        case QEvent::TabletLeaveProximity:
            break;
        default:
            qCWarning(KWIN_CORE) << "Unexpected tablet event type" << event;
            break;
        }
        return true;
    }

    bool tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time) override
    {
        KWaylandServer::TabletSeatV2Interface *tabletSeat = findTabletSeat();
        auto tool = tabletSeat->toolByHardwareSerial(tabletToolId.m_serialId, getType(tabletToolId));
        if (!tool) {
            tool = createTool(tabletToolId);
        }
        if (!tool->isClientSupported()) {
            return false;
        }
        tool->sendButton(button, pressed);
        return true;
    }

    KWaylandServer::TabletPadV2Interface *findAndAdoptPad(const TabletPadId &tabletPadId) const
    {
        Window *window = workspace()->activeWindow();
        auto seat = findTabletSeat();
        if (!window || !window->surface() || !seat->isClientSupported(window->surface()->client())) {
            return nullptr;
        }

        auto tablet = static_cast<KWaylandServer::TabletV2Interface *>(tabletPadId.data);
        KWaylandServer::SurfaceInterface *surface = window->surface();
        auto pad = tablet->pad();
        if (!pad) {
            return nullptr;
        }
        pad->setCurrentSurface(surface, tablet);
        return pad;
    }

    bool tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        auto pad = findAndAdoptPad(tabletPadId);
        if (!pad) {
            return false;
        }
        pad->sendButton(time, button, pressed);
        return true;
    }

    bool tabletPadRingEvent(int number, int angle, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        auto pad = findAndAdoptPad(tabletPadId);
        if (!pad) {
            return false;
        }
        auto ring = pad->ring(number);

        ring->sendAngle(angle);
        if (isFinger) {
            ring->sendSource(KWaylandServer::TabletPadRingV2Interface::SourceFinger);
        }
        ring->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
        return true;
    }

    bool tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        auto pad = findAndAdoptPad(tabletPadId);
        if (!pad) {
            return false;
        }
        auto strip = pad->strip(number);

        strip->sendPosition(position);
        if (isFinger) {
            strip->sendSource(KWaylandServer::TabletPadStripV2Interface::SourceFinger);
        }
        strip->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
        return true;
    }

    QHash<KWaylandServer::TabletToolV2Interface *, Cursor *> m_cursorByTool;
};

static KWaylandServer::AbstractDropHandler *dropHandler(Window *window)
{
    auto surface = window->surface();
    if (!surface) {
        return nullptr;
    }
    auto seat = waylandServer()->seat();
    auto dropTarget = seat->dropHandlerForSurface(surface);
    if (dropTarget) {
        return dropTarget;
    }

    if (qobject_cast<X11Window *>(window) && kwinApp()->xwayland()) {
        return kwinApp()->xwayland()->xwlDropHandler();
    }

    return nullptr;
}

class DragAndDropInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    DragAndDropInputFilter()
    {
        m_raiseTimer.setSingleShot(true);
        m_raiseTimer.setInterval(250);
        connect(&m_raiseTimer, &QTimer::timeout, this, &DragAndDropInputFilter::raiseDragTarget);
    }

    bool pointerEvent(MouseEvent *event, quint32 nativeButton) override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragPointer()) {
            return false;
        }
        if (seat->isDragTouch()) {
            return true;
        }
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove: {
            const auto pos = input()->globalPointer();
            seat->notifyPointerMotion(pos);
            seat->notifyPointerFrame();

            Window *dragTarget = pickDragTarget(pos);
            if (dragTarget) {
                if (dragTarget != m_dragTarget) {
                    workspace()->takeActivity(dragTarget, Workspace::ActivityFlag::ActivityFocus);
                    m_raiseTimer.start();
                }
                if ((pos - m_lastPos).manhattanLength() > 10) {
                    m_lastPos = pos;
                    // reset timer to delay raising the window
                    m_raiseTimer.start();
                }
            }
            m_dragTarget = dragTarget;

            if (auto *xwl = kwinApp()->xwayland()) {
                const auto ret = xwl->dragMoveFilter(dragTarget, event->globalPos());
                if (ret == Xwl::DragEventReply::Ignore) {
                    return false;
                } else if (ret == Xwl::DragEventReply::Take) {
                    break;
                }
            }

            if (dragTarget) {
                // TODO: consider decorations
                if (dragTarget->surface() != seat->dragSurface()) {
                    seat->setDragTarget(dropHandler(dragTarget), dragTarget->surface(), dragTarget->inputTransformation());
                }
            } else {
                // no window at that place, if we have a surface we need to reset
                seat->setDragTarget(nullptr, nullptr);
                m_dragTarget = nullptr;
            }
            break;
        }
        case QEvent::MouseButtonPress:
            seat->notifyPointerButton(nativeButton, KWaylandServer::PointerButtonState::Pressed);
            seat->notifyPointerFrame();
            break;
        case QEvent::MouseButtonRelease:
            raiseDragTarget();
            m_dragTarget = nullptr;
            seat->notifyPointerButton(nativeButton, KWaylandServer::PointerButtonState::Released);
            seat->notifyPointerFrame();
            break;
        default:
            break;
        }
        // TODO: should we pass through effects?
        return true;
    }

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isDragPointer()) {
            return true;
        }
        if (!seat->isDragTouch()) {
            return false;
        }
        if (m_touchId != id) {
            return true;
        }
        seat->setTimestamp(time);
        seat->notifyTouchDown(id, pos);
        m_lastPos = pos;
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isDragPointer()) {
            return true;
        }
        if (!seat->isDragTouch()) {
            return false;
        }
        if (m_touchId < 0) {
            // We take for now the first id appearing as a move after a drag
            // started. We can optimize by specifying the id the drag is
            // associated with by implementing a key-value getter in KWayland.
            m_touchId = id;
        }
        if (m_touchId != id) {
            return true;
        }
        seat->setTimestamp(time);
        seat->notifyTouchMotion(id, pos);

        if (Window *t = pickDragTarget(pos)) {
            // TODO: consider decorations
            if (t->surface() != seat->dragSurface()) {
                if ((m_dragTarget = static_cast<Window *>(t->isClient() ? t : nullptr))) {
                    workspace()->takeActivity(m_dragTarget, Workspace::ActivityFlag::ActivityFocus);
                    m_raiseTimer.start();
                }
                seat->setDragTarget(dropHandler(t), t->surface(), pos, t->inputTransformation());
            }
            if ((pos - m_lastPos).manhattanLength() > 10) {
                m_lastPos = pos;
                // reset timer to delay raising the window
                m_raiseTimer.start();
            }
        } else {
            // no window at that place, if we have a surface we need to reset
            seat->setDragTarget(nullptr, nullptr);
            m_dragTarget = nullptr;
        }
        return true;
    }
    bool touchUp(qint32 id, std::chrono::microseconds time) override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragTouch()) {
            return false;
        }
        seat->setTimestamp(time);
        seat->notifyTouchUp(id);
        if (m_touchId == id) {
            m_touchId = -1;
            raiseDragTarget();
        }
        return true;
    }
    bool keyEvent(KeyEvent *event) override
    {
        if (event->key() != Qt::Key_Escape) {
            return false;
        }

        auto seat = waylandServer()->seat();
        if (!seat->isDrag()) {
            return false;
        }
        seat->setTimestamp(event->timestamp());

        seat->cancelDrag();

        return true;
    }

private:
    void raiseDragTarget()
    {
        m_raiseTimer.stop();
        if (m_dragTarget) {
            workspace()->takeActivity(m_dragTarget, Workspace::ActivityFlag::ActivityRaise);
        }
    }

    Window *pickDragTarget(const QPointF &pos) const
    {
        const QList<Window *> stacking = workspace()->stackingOrder();
        if (stacking.isEmpty()) {
            return nullptr;
        }
        auto it = stacking.end();
        do {
            --it;
            Window *window = (*it);
            if (window->isDeleted()) {
                continue;
            }
            if (!window->isClient()) {
                continue;
            }
            if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop() || window->isMinimized() || window->isHiddenInternal()) {
                continue;
            }
            if (!window->readyForPainting()) {
                continue;
            }
            if (window->hitTest(pos)) {
                return window;
            }
        } while (it != stacking.begin());
        return nullptr;
    }

    qint32 m_touchId = -1;
    QPointF m_lastPos = QPointF(-1, -1);
    QPointer<Window> m_dragTarget;
    QTimer m_raiseTimer;
};

KWIN_SINGLETON_FACTORY(InputRedirection)

static const QString s_touchpadComponent = QStringLiteral("kcm_touchpad");

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
    , m_keyboard(new KeyboardInputRedirection(this))
    , m_pointer(new PointerInputRedirection(this))
    , m_tablet(new TabletInputRedirection(this))
    , m_touch(new TouchInputRedirection(this))
    , m_shortcuts(new GlobalShortcutsManager(this))
{
    qRegisterMetaType<KWin::InputRedirection::KeyboardKeyState>();
    qRegisterMetaType<KWin::InputRedirection::PointerButtonState>();
    qRegisterMetaType<KWin::InputRedirection::PointerAxis>();
    setupInputBackends();
    connect(kwinApp(), &Application::workspaceCreated, this, &InputRedirection::setupWorkspace);
}

InputRedirection::~InputRedirection()
{
    m_inputBackends.clear();
    m_inputDevices.clear();

    s_self = nullptr;
    qDeleteAll(m_filters);
    qDeleteAll(m_spies);
}

void InputRedirection::installInputEventFilter(InputEventFilter *filter)
{
    Q_ASSERT(!m_filters.contains(filter));
    m_filters << filter;
}

void InputRedirection::prependInputEventFilter(InputEventFilter *filter)
{
    Q_ASSERT(!m_filters.contains(filter));
    m_filters.prepend(filter);
}

void InputRedirection::uninstallInputEventFilter(InputEventFilter *filter)
{
    m_filters.removeOne(filter);
}

void InputRedirection::installInputEventSpy(InputEventSpy *spy)
{
    m_spies << spy;
}

void InputRedirection::uninstallInputEventSpy(InputEventSpy *spy)
{
    m_spies.removeOne(spy);
}

void InputRedirection::init()
{
    m_inputConfigWatcher = KConfigWatcher::create(InputConfig::self()->inputConfig());
    connect(m_inputConfigWatcher.data(), &KConfigWatcher::configChanged,
            this, &InputRedirection::handleInputConfigChanged);

    m_shortcuts->init();
}

void InputRedirection::setupWorkspace()
{
    connect(workspace(), &Workspace::outputsChanged, this, &InputRedirection::updateScreens);
    if (waylandServer()) {
        m_keyboard->init();
        m_pointer->init();
        m_touch->init();
        m_tablet->init();

        updateLeds(m_keyboard->xkb()->leds());
        connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, this, &InputRedirection::updateLeds);

        setupTouchpadShortcuts();
        setupInputFilters();
        updateScreens();
    }
}

void InputRedirection::updateScreens()
{
    for (const auto &backend : m_inputBackends) {
        backend->updateScreens();
    }
}

QObject *InputRedirection::lastInputHandler() const
{
    return m_lastInputDevice;
}

void InputRedirection::setLastInputHandler(QObject *device)
{
    m_lastInputDevice = device;
}

class WindowInteractedSpy : public InputEventSpy
{
public:
    void keyEvent(KeyEvent *event) override
    {
        if (event->isAutoRepeat() || event->type() != QEvent::KeyPress) {
            return;
        }
        update();
    }

    void pointerEvent(KWin::MouseEvent *event) override
    {
        if (event->type() != QEvent::MouseButtonPress) {
            return;
        }
        update();
    }

    void tabletPadButtonEvent(uint, bool pressed, const KWin::TabletPadId &, std::chrono::microseconds time) override
    {
        if (!pressed) {
            return;
        }
        update();
    }

    void tabletToolButtonEvent(uint, bool pressed, const KWin::TabletToolId &, std::chrono::microseconds time) override
    {
        if (!pressed) {
            return;
        }
        update();
    }

    void tabletToolEvent(KWin::TabletEvent *event) override
    {
        if (event->type() != QEvent::TabletPress) {
            return;
        }
        update();
    }

    void touchDown(qint32, const QPointF &, std::chrono::microseconds time) override
    {
        update();
    }

    void update()
    {
        auto window = workspace()->activeWindow();
        if (!window) {
            return;
        }
        window->setLastUsageSerial(waylandServer()->seat()->display()->serial());
    }
};

class UserActivitySpy : public InputEventSpy
{
public:
    void pointerEvent(MouseEvent *event) override
    {
        notifyActivity();
    }
    void wheelEvent(WheelEvent *event) override
    {
        notifyActivity();
    }

    void keyEvent(KeyEvent *event) override
    {
        notifyActivity();
    }

    void touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void touchUp(qint32 id, std::chrono::microseconds time) override
    {
        notifyActivity();
    }

    void pinchGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void pinchGestureEnd(std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void pinchGestureCancelled(std::chrono::microseconds time) override
    {
        notifyActivity();
    }

    void swipeGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void swipeGestureEnd(std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void swipeGestureCancelled(std::chrono::microseconds time) override
    {
        notifyActivity();
    }

    void holdGestureBegin(int fingerCount, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void holdGestureEnd(std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void holdGestureCancelled(std::chrono::microseconds time) override
    {
        notifyActivity();
    }

    void tabletToolEvent(TabletEvent *event) override
    {
        notifyActivity();
    }
    void tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        notifyActivity();
    }
    void tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId, std::chrono::microseconds time) override
    {
        notifyActivity();
    }

private:
    void notifyActivity()
    {
        input()->simulateUserActivity();
    }
};

void InputRedirection::setupInputFilters()
{
    const bool hasGlobalShortcutSupport = waylandServer()->hasGlobalShortcutSupport();
    if (kwinApp()->session()->capabilities() & Session::Capability::SwitchTerminal)
        {
        installInputEventFilter(new VirtualTerminalFilter);
    }
    installInputEventSpy(new HideCursorSpy);
    installInputEventSpy(new UserActivitySpy);
    installInputEventSpy(new WindowInteractedSpy);
    if (hasGlobalShortcutSupport) {
        installInputEventFilter(new TerminateServerFilter);
    }
    installInputEventFilter(new DragAndDropInputFilter);
    installInputEventFilter(new LockScreenFilter);
    m_windowSelector = new WindowSelectorFilter;
    installInputEventFilter(m_windowSelector);
    if (hasGlobalShortcutSupport) {
        installInputEventFilter(new ScreenEdgeInputFilter);
    }
#if KWIN_BUILD_TABBOX
    installInputEventFilter(new TabBoxInputFilter);
#endif
    if (hasGlobalShortcutSupport) {
        installInputEventFilter(new GlobalShortcutFilter);
    }
    installInputEventFilter(new EffectsFilter);
    installInputEventFilter(new MoveResizeFilter);
    installInputEventFilter(new PopupInputFilter);
    installInputEventFilter(new DecorationEventFilter);
    installInputEventFilter(new WindowActionInputFilter);
    installInputEventFilter(new InternalWindowEventFilter);
    installInputEventFilter(new InputKeyboardFilter);
    installInputEventFilter(new ForwardInputFilter);
    installInputEventFilter(new TabletInputFilter);
}

void InputRedirection::handleInputConfigChanged(const KConfigGroup &group)
{
    if (group.name() == QLatin1String("Keyboard")) {
        m_keyboard->reconfigure();
    }
}

void InputRedirection::updateLeds(LEDs leds)
{
    if (m_leds != leds) {
        m_leds = leds;

        for (InputDevice *device : std::as_const(m_inputDevices)) {
            device->setLeds(leds);
        }
    }
}

void InputRedirection::addInputDevice(InputDevice *device)
{
    connect(device, &InputDevice::keyChanged, m_keyboard, &KeyboardInputRedirection::processKey);

    connect(device, &InputDevice::pointerMotionAbsolute,
            m_pointer, &PointerInputRedirection::processMotionAbsolute);
    connect(device, &InputDevice::pointerMotion,
            m_pointer, &PointerInputRedirection::processMotion);
    connect(device, &InputDevice::pointerButtonChanged,
            m_pointer, &PointerInputRedirection::processButton);
    connect(device, &InputDevice::pointerAxisChanged,
            m_pointer, &PointerInputRedirection::processAxis);
    connect(device, &InputDevice::pinchGestureBegin,
            m_pointer, &PointerInputRedirection::processPinchGestureBegin);
    connect(device, &InputDevice::pinchGestureUpdate,
            m_pointer, &PointerInputRedirection::processPinchGestureUpdate);
    connect(device, &InputDevice::pinchGestureEnd,
            m_pointer, &PointerInputRedirection::processPinchGestureEnd);
    connect(device, &InputDevice::pinchGestureCancelled,
            m_pointer, &PointerInputRedirection::processPinchGestureCancelled);
    connect(device, &InputDevice::swipeGestureBegin,
            m_pointer, &PointerInputRedirection::processSwipeGestureBegin);
    connect(device, &InputDevice::swipeGestureUpdate,
            m_pointer, &PointerInputRedirection::processSwipeGestureUpdate);
    connect(device, &InputDevice::swipeGestureEnd,
            m_pointer, &PointerInputRedirection::processSwipeGestureEnd);
    connect(device, &InputDevice::swipeGestureCancelled,
            m_pointer, &PointerInputRedirection::processSwipeGestureCancelled);
    connect(device, &InputDevice::holdGestureBegin,
            m_pointer, &PointerInputRedirection::processHoldGestureBegin);
    connect(device, &InputDevice::holdGestureEnd,
            m_pointer, &PointerInputRedirection::processHoldGestureEnd);
    connect(device, &InputDevice::holdGestureCancelled,
            m_pointer, &PointerInputRedirection::processHoldGestureCancelled);

    connect(device, &InputDevice::touchDown, m_touch, &TouchInputRedirection::processDown);
    connect(device, &InputDevice::touchUp, m_touch, &TouchInputRedirection::processUp);
    connect(device, &InputDevice::touchMotion, m_touch, &TouchInputRedirection::processMotion);
    connect(device, &InputDevice::touchCanceled, m_touch, &TouchInputRedirection::cancel);
    connect(device, &InputDevice::touchFrame, m_touch, &TouchInputRedirection::frame);

    auto handleSwitchEvent = [this](SwitchEvent::State state, std::chrono::microseconds time, InputDevice *device) {
        SwitchEvent event(state, time, device);
        processSpies(std::bind(&InputEventSpy::switchEvent, std::placeholders::_1, &event));
        processFilters(std::bind(&InputEventFilter::switchEvent, std::placeholders::_1, &event));
    };
    connect(device, &InputDevice::switchToggledOn, this,
            std::bind(handleSwitchEvent, SwitchEvent::State::On, std::placeholders::_1, std::placeholders::_2));
    connect(device, &InputDevice::switchToggledOff, this,
            std::bind(handleSwitchEvent, SwitchEvent::State::Off, std::placeholders::_1, std::placeholders::_2));

    connect(device, &InputDevice::tabletToolEvent,
            m_tablet, &TabletInputRedirection::tabletToolEvent);
    connect(device, &InputDevice::tabletToolButtonEvent,
            m_tablet, &TabletInputRedirection::tabletToolButtonEvent);
    connect(device, &InputDevice::tabletPadButtonEvent,
            m_tablet, &TabletInputRedirection::tabletPadButtonEvent);
    connect(device, &InputDevice::tabletPadRingEvent,
            m_tablet, &TabletInputRedirection::tabletPadRingEvent);
    connect(device, &InputDevice::tabletPadStripEvent,
            m_tablet, &TabletInputRedirection::tabletPadStripEvent);

    device->setLeds(m_leds);

    m_inputDevices.append(device);
    Q_EMIT deviceAdded(device);

    updateAvailableInputDevices();
}

void InputRedirection::removeInputDevice(InputDevice *device)
{
    m_inputDevices.removeOne(device);
    Q_EMIT deviceRemoved(device);

    updateAvailableInputDevices();
}

void InputRedirection::updateAvailableInputDevices()
{
    const bool hasKeyboard = std::any_of(m_inputDevices.constBegin(), m_inputDevices.constEnd(), [](InputDevice *device) {
        return device->isKeyboard();
    });
    if (m_hasKeyboard != hasKeyboard) {
        m_hasKeyboard = hasKeyboard;
        Q_EMIT hasKeyboardChanged(hasKeyboard);
    }

    const bool hasAlphaNumericKeyboard = std::any_of(m_inputDevices.constBegin(), m_inputDevices.constEnd(), [](InputDevice *device) {
        return device->isAlphaNumericKeyboard();
    });
    if (m_hasAlphaNumericKeyboard != hasAlphaNumericKeyboard) {
        m_hasAlphaNumericKeyboard = hasAlphaNumericKeyboard;
        Q_EMIT hasAlphaNumericKeyboardChanged(hasAlphaNumericKeyboard);
    }

    const bool hasPointer = std::any_of(m_inputDevices.constBegin(), m_inputDevices.constEnd(), [](InputDevice *device) {
        return device->isPointer();
    });
    if (m_hasPointer != hasPointer) {
        m_hasPointer = hasPointer;
        Q_EMIT hasPointerChanged(hasPointer);
    }

    const bool hasTouch = std::any_of(m_inputDevices.constBegin(), m_inputDevices.constEnd(), [](InputDevice *device) {
        return device->isTouch();
    });
    if (m_hasTouch != hasTouch) {
        m_hasTouch = hasTouch;
        Q_EMIT hasTouchChanged(hasTouch);
    }

    const bool hasTabletModeSwitch = std::any_of(m_inputDevices.constBegin(), m_inputDevices.constEnd(), [](InputDevice *device) {
        return device->isTabletModeSwitch();
    });
    if (m_hasTabletModeSwitch != hasTabletModeSwitch) {
        m_hasTabletModeSwitch = hasTabletModeSwitch;
        Q_EMIT hasTabletModeSwitchChanged(hasTabletModeSwitch);
    }
}

void InputRedirection::toggleTouchpads()
{
    bool changed = false;
    m_touchpadsEnabled = !m_touchpadsEnabled;
    for (InputDevice *device : std::as_const(m_inputDevices)) {
        if (!device->isTouchpad()) {
            continue;
        }
        const bool old = device->isEnabled();
        device->setEnabled(m_touchpadsEnabled);
        if (old != device->isEnabled()) {
            changed = true;
        }
    }
    if (changed) {
        // send OSD message
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.plasmashell"),
                                                          QStringLiteral("/org/kde/osdService"),
                                                          QStringLiteral("org.kde.osdService"),
                                                          QStringLiteral("touchpadEnabledChanged"));
        msg.setArguments({m_touchpadsEnabled});
        QDBusConnection::sessionBus().asyncCall(msg);
    }
}

void InputRedirection::enableTouchpads()
{
    if (!m_touchpadsEnabled) {
        toggleTouchpads();
    }
}

void InputRedirection::disableTouchpads()
{
    if (m_touchpadsEnabled) {
        toggleTouchpads();
    }
}

void InputRedirection::addInputBackend(std::unique_ptr<InputBackend> &&inputBackend)
{
    connect(inputBackend.get(), &InputBackend::deviceAdded, this, &InputRedirection::addInputDevice);
    connect(inputBackend.get(), &InputBackend::deviceRemoved, this, &InputRedirection::removeInputDevice);

    inputBackend->setConfig(InputConfig::self()->inputConfig());
    inputBackend->initialize();

    m_inputBackends.push_back(std::move(inputBackend));
}

void InputRedirection::setupInputBackends()
{
    std::unique_ptr<InputBackend> inputBackend = kwinApp()->outputBackend()->createInputBackend();
    if (inputBackend) {
        addInputBackend(std::move(inputBackend));
    }
    if (waylandServer()) {
        addInputBackend(std::make_unique<FakeInputBackend>());
    }
}

void InputRedirection::setupTouchpadShortcuts()
{
    QAction *touchpadToggleAction = new QAction(this);
    QAction *touchpadOnAction = new QAction(this);
    QAction *touchpadOffAction = new QAction(this);

    const QString touchpadDisplayName = i18n("Touchpad");

    touchpadToggleAction->setObjectName(QStringLiteral("Toggle Touchpad"));
    touchpadToggleAction->setProperty("componentName", s_touchpadComponent);
    touchpadToggleAction->setProperty("componentDisplayName", touchpadDisplayName);
    touchpadOnAction->setObjectName(QStringLiteral("Enable Touchpad"));
    touchpadOnAction->setProperty("componentName", s_touchpadComponent);
    touchpadOnAction->setProperty("componentDisplayName", touchpadDisplayName);
    touchpadOffAction->setObjectName(QStringLiteral("Disable Touchpad"));
    touchpadOffAction->setProperty("componentName", s_touchpadComponent);
    touchpadOffAction->setProperty("componentDisplayName", touchpadDisplayName);
    KGlobalAccel::self()->setDefaultShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle, Qt::ControlModifier | Qt::MetaModifier | Qt::Key_Zenkaku_Hankaku});
    KGlobalAccel::self()->setShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle, Qt::ControlModifier | Qt::MetaModifier | Qt::Key_Zenkaku_Hankaku});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
    KGlobalAccel::self()->setShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
    connect(touchpadToggleAction, &QAction::triggered, this, &InputRedirection::toggleTouchpads);
    connect(touchpadOnAction, &QAction::triggered, this, &InputRedirection::enableTouchpads);
    connect(touchpadOffAction, &QAction::triggered, this, &InputRedirection::disableTouchpads);
}

bool InputRedirection::hasAlphaNumericKeyboard()
{
    return m_hasAlphaNumericKeyboard;
}

bool InputRedirection::hasPointer() const
{
    return m_hasPointer;
}

bool InputRedirection::hasTouch() const
{
    return m_hasTouch;
}

bool InputRedirection::hasTabletModeSwitch()
{
    return m_hasTabletModeSwitch;
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    return m_pointer->buttons();
}

void InputRedirection::simulateUserActivity()
{
    for (IdleDetector *idleDetector : std::as_const(m_idleDetectors)) {
        idleDetector->activity();
    }
}

void InputRedirection::addIdleDetector(IdleDetector *detector)
{
    Q_ASSERT(!m_idleDetectors.contains(detector));
    detector->setInhibited(!m_idleInhibitors.isEmpty());
    m_idleDetectors.append(detector);
}

void InputRedirection::removeIdleDetector(IdleDetector *detector)
{
    m_idleDetectors.removeOne(detector);
}

QList<Window *> InputRedirection::idleInhibitors() const
{
    return m_idleInhibitors;
}

void InputRedirection::addIdleInhibitor(Window *inhibitor)
{
    if (!m_idleInhibitors.contains(inhibitor)) {
        m_idleInhibitors.append(inhibitor);
        for (IdleDetector *idleDetector : std::as_const(m_idleDetectors)) {
            idleDetector->setInhibited(true);
        }
    }
}

void InputRedirection::removeIdleInhibitor(Window *inhibitor)
{
    if (m_idleInhibitors.removeOne(inhibitor) && m_idleInhibitors.isEmpty()) {
        for (IdleDetector *idleDetector : std::as_const(m_idleDetectors)) {
            idleDetector->setInhibited(false);
        }
    }
}

Window *InputRedirection::findToplevel(const QPointF &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
    if (!isScreenLocked) {
        // if an effect overrides the cursor we don't have a window to focus
        if (effects && static_cast<EffectsHandlerImpl *>(effects)->isMouseInterception()) {
            return nullptr;
        }
    }
    const QList<Window *> &stacking = Workspace::self()->stackingOrder();
    if (stacking.isEmpty()) {
        return nullptr;
    }
    auto it = stacking.end();
    do {
        --it;
        Window *window = (*it);
        if (window->isDeleted()) {
            // a deleted window doesn't get mouse events
            continue;
        }
        if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop() || window->isMinimized() || window->isHiddenInternal()) {
            continue;
        }
        if (!window->readyForPainting()) {
            continue;
        }
        if (isScreenLocked) {
            if (!window->isLockScreen() && !window->isInputMethod() && !window->isLockScreenOverlay()) {
                continue;
            }
        }
        if (window->hitTest(pos)) {
            return window;
        }
    } while (it != stacking.begin());
    return nullptr;
}

Qt::KeyboardModifiers InputRedirection::keyboardModifiers() const
{
    return m_keyboard->modifiers();
}

Qt::KeyboardModifiers InputRedirection::modifiersRelevantForGlobalShortcuts() const
{
    return m_keyboard->modifiersRelevantForGlobalShortcuts();
}

void InputRedirection::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
}

void InputRedirection::registerRealtimeTouchpadSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> cb)
{
    m_shortcuts->registerRealtimeTouchpadSwipe(action, cb, direction, fingerCount);
}

void InputRedirection::registerTouchpadSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action)
{
    m_shortcuts->registerTouchpadSwipe(action, direction, fingerCount);
}

void InputRedirection::registerTouchpadPinchShortcut(PinchDirection direction, uint fingerCount, QAction *action)
{
    m_shortcuts->registerTouchpadPinch(action, direction, fingerCount);
}

void InputRedirection::registerRealtimeTouchpadPinchShortcut(PinchDirection direction, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)
{
    m_shortcuts->registerRealtimeTouchpadPinch(onUp, progressCallback, direction, fingerCount);
}

void InputRedirection::registerGlobalAccel(KGlobalAccelInterface *interface)
{
    m_shortcuts->setKGlobalAccelInterface(interface);
}

void InputRedirection::registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback)
{
    m_shortcuts->registerTouchscreenSwipe(action, progressCallback, direction, fingerCount);
}

void InputRedirection::forceRegisterTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback)
{
    m_shortcuts->forceRegisterTouchscreenSwipe(action, progressCallback, direction, fingerCount);
}

void InputRedirection::warpPointer(const QPointF &pos)
{
    m_pointer->warp(pos);
}

bool InputRedirection::supportsPointerWarping() const
{
    return m_pointer->supportsWarping();
}

QPointF InputRedirection::globalPointer() const
{
    return m_pointer->pos();
}

void InputRedirection::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    if (!m_windowSelector || m_windowSelector->isActive()) {
        callback(nullptr);
        return;
    }
    m_windowSelector->start(callback);
    m_pointer->setWindowSelectionCursor(cursorName);
}

void InputRedirection::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    if (!m_windowSelector || m_windowSelector->isActive()) {
        callback(QPoint(-1, -1));
        return;
    }
    m_windowSelector->start(callback);
    m_pointer->setWindowSelectionCursor(QByteArray());
}

bool InputRedirection::isSelectingWindow() const
{
    return m_windowSelector ? m_windowSelector->isActive() : false;
}

InputDeviceHandler::InputDeviceHandler(InputRedirection *input)
    : QObject(input)
{
}

InputDeviceHandler::~InputDeviceHandler() = default;

void InputDeviceHandler::init()
{
    connect(workspace(), &Workspace::stackingOrderChanged, this, &InputDeviceHandler::update);
    connect(workspace(), &Workspace::windowMinimizedChanged, this, &InputDeviceHandler::update);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, this, &InputDeviceHandler::update);
}

bool InputDeviceHandler::setHover(Window *window)
{
    if (m_hover.window == window) {
        return false;
    }
    auto old = m_hover.window;
    disconnect(m_hover.surfaceCreatedConnection);
    m_hover.surfaceCreatedConnection = QMetaObject::Connection();

    m_hover.window = window;
    return true;
}

void InputDeviceHandler::setFocus(Window *window)
{
    if (m_focus.window != window) {
        Window *oldFocus = m_focus.window;
        m_focus.window = window;
        focusUpdate(oldFocus, m_focus.window);
    }
}

void InputDeviceHandler::setDecoration(Decoration::DecoratedClientImpl *decoration)
{
    if (m_focus.decoration != decoration) {
        auto oldDeco = m_focus.decoration;
        m_focus.decoration = decoration;
        cleanupDecoration(oldDeco.data(), m_focus.decoration.data());
        Q_EMIT decorationChanged();
    }
}

void InputDeviceHandler::updateFocus()
{
    Window *focus = m_hover.window;

    if (m_focus.decoration) {
        focus = nullptr;
    } else if (m_hover.window && !m_hover.window->surface() && !m_hover.window->isInternal()) {
        // The surface has not yet been created (special XWayland case).
        // Therefore listen for its creation.
        if (!m_hover.surfaceCreatedConnection) {
            m_hover.surfaceCreatedConnection = connect(m_hover.window, &Window::surfaceChanged,
                                                       this, &InputDeviceHandler::update);
        }
        focus = nullptr;
    }

    setFocus(focus);
}

void InputDeviceHandler::updateDecoration()
{
    Decoration::DecoratedClientImpl *decoration = nullptr;
    auto hover = m_hover.window.data();
    if (hover && hover->decoratedClient()) {
        if (!hover->clientGeometry().toRect().contains(flooredPoint(position()))) {
            // input device above decoration
            decoration = hover->decoratedClient();
        }
    }

    setDecoration(decoration);
}

void InputDeviceHandler::update()
{
    if (!m_inited) {
        return;
    }

    Window *window = nullptr;
    if (positionValid()) {
        window = input()->findToplevel(position());
    }
    // Always set the window at the position of the input device.
    setHover(window);

    if (focusUpdatesBlocked()) {
        workspace()->updateFocusMousePosition(position());
        return;
    }

    updateDecoration();
    updateFocus();

    workspace()->updateFocusMousePosition(position());
}

Window *InputDeviceHandler::hover() const
{
    return m_hover.window.data();
}

Window *InputDeviceHandler::focus() const
{
    return m_focus.window.data();
}

Decoration::DecoratedClientImpl *InputDeviceHandler::decoration() const
{
    return m_focus.decoration;
}

} // namespace

#include "input.moc"
