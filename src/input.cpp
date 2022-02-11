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
#include "effects.h"
#include "gestures.h"
#include "globalshortcuts.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "inputbackend.h"
#include "inputmethod.h"
#include "keyboard_input.h"
#include "main.h"
#include "pointer_input.h"
#include "session.h"
#include "tablet_input.h"
#include "hide_cursor_spy.h"
#include "touch_input.h"
#include "x11client.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "internal_client.h"
#include "platform.h"
#include "popup_input_filter.h"
#include "screenedge.h"
#include "screens.h"
#include "unmanaged.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwl/xwayland_interface.h"
#include "cursor.h"
#include <KDecoration2/Decoration>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KWaylandServer/display.h>
#include <KWaylandServer/inputmethod_v1_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/shmclientbuffer.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/tablet_v2_interface.h>
#include <decorations/decoratedclient.h>

//screenlocker
#include <KScreenLocker/KsldApp>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeyEvent>
#include <QThread>
#include <qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon.h>

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

bool InputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    return false;
}

bool InputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::keyEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::touchDown(qint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchMotion(qint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    Q_UNUSED(scale)
    Q_UNUSED(angleDelta)
    Q_UNUSED(delta)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureEnd(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureUpdate(const QSizeF &delta, quint32 time)
{
    Q_UNUSED(delta)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureEnd(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::holdGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::holdGestureEnd(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::holdGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::switchEvent(SwitchEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::tabletToolEvent(TabletEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletId)
{
    Q_UNUSED(button)
    Q_UNUSED(pressed)
    Q_UNUSED(tabletId)
    return false;
}

bool InputEventFilter::tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId)
{
    Q_UNUSED(button)
    Q_UNUSED(pressed)
    Q_UNUSED(tabletPadId)
    return false;
}

bool InputEventFilter::tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    Q_UNUSED(tabletPadId)
    return false;
}

bool InputEventFilter::tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    Q_UNUSED(tabletPadId)
    return false;
}

void InputEventFilter::passToWaylandServer(QKeyEvent *event)
{
    Q_ASSERT(waylandServer());
    if (event->isAutoRepeat()) {
        return;
    }

    KWaylandServer::SeatInterface *seat = waylandServer()->seat();
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
}

bool InputEventFilter::passToInputMethod(QKeyEvent *event)
{
    auto *inputmethod = InputMethod::self();

    if (!inputmethod) {
        return false;
    }

    if (auto keyboardGrab = inputmethod->keyboardGrab()) {
        if (event->isAutoRepeat()) {
            return true;
        }
        auto newState = event->type() == QEvent::KeyPress ? KWaylandServer::KeyboardKeyState::Pressed : KWaylandServer::KeyboardKeyState::Released;
        keyboardGrab->sendKey(waylandServer()->display()->nextSerial(), event->timestamp(), event->nativeScanCode(), newState);
        return true;
    }
    return false;
}

class VirtualTerminalFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
        // really on press and not on release? X11 switches on press.
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            const xkb_keysym_t keysym = event->nativeVirtualKey();
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                kwinApp()->platform()->session()->switchTo(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
                return true;
            }
        }
        return false;
    }
};

class TerminateServerFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
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

class LockScreenFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        auto client = qobject_cast<AbstractClient *>(input()->findToplevel(event->globalPos()));
        if (client && client->isLockScreen()) {
            workspace()->activateClient(client);
        }

        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (event->type() == QEvent::MouseMove) {
            if (pointerSurfaceAllowed()) {
                // TODO: should the pointer position always stay in sync, i.e. not do the check?
                seat->notifyPointerMotion(event->screenPos().toPoint());
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
    bool wheelEvent(QWheelEvent *event) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            const WheelEvent *wheelEvent = static_cast<WheelEvent *>(event);
            seat->setTimestamp(wheelEvent->timestamp());
            seat->notifyPointerAxis(wheelEvent->orientation(), wheelEvent->delta(),
                                    wheelEvent->discreteDelta(),
                                    kwinAxisSourceToKWaylandAxisSource(wheelEvent->axisSource()));
            seat->notifyPointerFrame();
        }
        return true;
    }
    bool keyEvent(QKeyEvent * event) override {
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
        event->setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), event);
        if (event->isAccepted()) {
            return true;
        }

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
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
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
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
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
    bool touchUp(qint32 id, quint32 time) override {
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
    bool pinchGestureBegin(int fingerCount, quint32 time) override {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override {
        Q_UNUSED(scale)
        Q_UNUSED(angleDelta)
        Q_UNUSED(delta)
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureEnd(quint32 time) override {
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureCancelled(quint32 time) override {
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }

    bool swipeGestureBegin(int fingerCount, quint32 time) override {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureUpdate(const QSizeF &delta, quint32 time) override {
        Q_UNUSED(delta)
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureEnd(quint32 time) override {
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureCancelled(quint32 time) override {
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureBegin(int fingerCount, quint32 time) override {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureEnd(quint32 time) override {
        Q_UNUSED(time)
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
private:
    bool surfaceAllowed(KWaylandServer::SurfaceInterface *(KWaylandServer::SeatInterface::*method)() const) const {
        if (KWaylandServer::SurfaceInterface *s = (waylandServer()->seat()->*method)()) {
            if (Toplevel *t = waylandServer()->findClient(s)) {
                return t->isLockScreen() || t->isInputMethod();
            }
            return false;
        }
        return true;
    }
    bool pointerSurfaceAllowed() const {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedPointerSurface);
    }
    bool keyboardSurfaceAllowed() const {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedKeyboardSurface);
    }
    bool touchSurfaceAllowed() const {
        return surfaceAllowed(&KWaylandServer::SeatInterface::focusedTouchSurface);
    }
};

class EffectsFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event);
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!effects || !static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        passToWaylandServer(event);
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(event);
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchDown(id, pos, time);
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchMotion(id, pos, time);
    }
    bool touchUp(qint32 id, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchUp(id, time);
    }
};

class MoveResizeFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseMove:
            c->updateInteractiveMoveResize(event->screenPos().toPoint());
            break;
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                c->endInteractiveMoveResize();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        Q_UNUSED(event)
        // filter out while moving a window
        return workspace()->moveResizeClient() != nullptr;
    }
    bool keyEvent(QKeyEvent *event) override {
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            c->keyPressEvent(event->key() | event->modifiers());
            if (c->isInteractiveMove() || c->isInteractiveResize()) {
                // only update if mode didn't end
                c->updateInteractiveMoveResize(input()->globalPointer());
            }
        }
        return true;
    }

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(id)
        Q_UNUSED(pos)
        Q_UNUSED(time)
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        return true;
    }

    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        if (!m_set) {
            m_id = id;
            m_set = true;
        }
        if (m_id == id) {
            c->updateInteractiveMoveResize(pos.toPoint());
        }
        return true;
    }

    bool touchUp(qint32 id, quint32 time) override {
        Q_UNUSED(time)
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        if (m_id == id || !m_set) {
            c->endInteractiveMoveResize();
            m_set = false;
            // pass through to update decoration filter later on
            return false;
        }
        m_set = false;
        return true;
    }

    bool tabletToolEvent(TabletEvent *event) override {
        AbstractClient *c = workspace()->moveResizeClient();
        if (!c) {
            return false;
        }
        switch (event->type()) {
        case QEvent::TabletMove:
            c->updateInteractiveMoveResize(event->globalPos());
            break;
        case QEvent::TabletRelease:
            c->endInteractiveMoveResize();
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

class WindowSelectorFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
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
    bool wheelEvent(QWheelEvent *event) override {
        Q_UNUSED(event)
        // filter out while selecting a window
        return m_active;
    }
    bool keyEvent(QKeyEvent *event) override {
        Q_UNUSED(event)
        if (!m_active) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        passToWaylandServer(event);

        if (event->type() == QEvent::KeyPress) {
            // x11 variant does this on key press, so do the same
            if (event->key() == Qt::Key_Escape) {
                cancel();
            } else if (event->key() == Qt::Key_Enter ||
                       event->key() == Qt::Key_Return ||
                       event->key() == Qt::Key_Space) {
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

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        if (!isActive()) {
            return false;
        }
        m_touchPoints.insert(id, pos);
        return true;
    }

    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        if (!isActive()) {
            return false;
        }
        auto it = m_touchPoints.find(id);
        if (it != m_touchPoints.end()) {
            *it = pos;
        }
        return true;
    }

    bool touchUp(qint32 id, quint32 time) override {
        Q_UNUSED(time)
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

    bool isActive() const {
        return m_active;
    }
    void start(std::function<void(KWin::Toplevel*)> callback) {
        Q_ASSERT(!m_active);
        m_active = true;
        m_callback = callback;
        input()->keyboard()->update();
        input()->touch()->cancel();
    }
    void start(std::function<void(const QPoint &)> callback) {
        Q_ASSERT(!m_active);
        m_active = true;
        m_pointSelectionFallback = callback;
        input()->keyboard()->update();
        input()->touch()->cancel();
    }
private:
    void deactivate() {
        m_active = false;
        m_callback = std::function<void(KWin::Toplevel*)>();
        m_pointSelectionFallback = std::function<void(const QPoint &)>();
        input()->pointer()->removeWindowSelectionCursor();
        input()->keyboard()->update();
        m_touchPoints.clear();
    }
    void cancel() {
        if (m_callback) {
            m_callback(nullptr);
        }
        if (m_pointSelectionFallback) {
            m_pointSelectionFallback(QPoint(-1, -1));
        }
        deactivate();
    }
    void accept(const QPoint &pos) {
        if (m_callback) {
            // TODO: this ignores shaped windows
            m_callback(input()->findToplevel(pos));
        }
        if (m_pointSelectionFallback) {
            m_pointSelectionFallback(pos);
        }
        deactivate();
    }
    void accept(const QPointF &pos) {
        accept(pos.toPoint());
    }
    bool m_active = false;
    std::function<void(KWin::Toplevel*)> m_callback;
    std::function<void(const QPoint &)> m_pointSelectionFallback;
    QMap<quint32, QPointF> m_touchPoints;
};

class GlobalShortcutFilter : public InputEventFilter {
public:
    GlobalShortcutFilter() {
        m_powerDown = new QTimer;
        m_powerDown->setSingleShot(true);
        m_powerDown->setInterval(1000);
    }
    ~GlobalShortcutFilter() {
        delete m_powerDown;
    }

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton);
        if (event->type() == QEvent::MouseButtonPress) {
            if (input()->shortcuts()->processPointerPressed(event->modifiers(), event->buttons())) {
                return true;
            }
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
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
    bool keyEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_PowerOff) {
            const auto modifiers = static_cast<KeyEvent*>(event)->modifiersRelevantForGlobalShortcuts();
            if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
                QObject::connect(m_powerDown, &QTimer::timeout, input()->shortcuts(), [this, modifiers] {
                    QObject::disconnect(m_powerDown, &QTimer::timeout, input()->shortcuts(), nullptr);
                    m_powerDown->stop();
                    input()->shortcuts()->processKey(modifiers, Qt::Key_PowerDown);
                });
                m_powerDown->start();
                return true;
            } else if (event->type() == QEvent::KeyRelease) {
                const bool ret = !m_powerDown->isActive() || input()->shortcuts()->processKey(modifiers, event->key());
                m_powerDown->stop();
                return ret;
            }
        } else if (event->type() == QEvent::KeyPress) {
            if (!waylandServer()->isKeyboardShortcutsInhibited()) {
                return input()->shortcuts()->processKey(static_cast<KeyEvent*>(event)->modifiersRelevantForGlobalShortcuts(), event->key());
            }
        }
        return false;
    }
    bool swipeGestureBegin(int fingerCount, quint32 time) override {
        Q_UNUSED(time)
        input()->shortcuts()->processSwipeStart(fingerCount);
        return false;
    }
    bool swipeGestureUpdate(const QSizeF &delta, quint32 time) override {
        Q_UNUSED(time)
        input()->shortcuts()->processSwipeUpdate(delta);
        return false;
    }
    bool swipeGestureCancelled(quint32 time) override {
        Q_UNUSED(time)
        input()->shortcuts()->processSwipeCancel();
        return false;
    }
    bool swipeGestureEnd(quint32 time) override {
        Q_UNUSED(time)
        input()->shortcuts()->processSwipeEnd();
        return false;
    }

private:
    QTimer* m_powerDown = nullptr;
};


namespace {

enum class MouseAction {
    ModifierOnly,
    ModifierAndWindow
};
std::pair<bool, bool> performClientMouseAction(QMouseEvent *event, AbstractClient *client, MouseAction action = MouseAction::ModifierOnly)
{
    Options::MouseCommand command = Options::MouseNothing;
    bool wasAction = false;
    if (static_cast<MouseEvent*>(event)->modifiersRelevantForGlobalShortcuts() == options->commandAllModifier()) {
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
            command = client->getMouseCommand(event->button(), &wasAction);
        }
    }
    if (wasAction) {
        return std::make_pair(wasAction, !client->performMouseCommand(command, event->globalPos()));
    }
    return std::make_pair(wasAction, false);
}

std::pair<bool, bool> performClientWheelAction(QWheelEvent *event, AbstractClient *c, MouseAction action = MouseAction::ModifierOnly)
{
    bool wasAction = false;
    Options::MouseCommand command = Options::MouseNothing;
    if (static_cast<WheelEvent*>(event)->modifiersRelevantForGlobalShortcuts() == options->commandAllModifier()) {
        if (!input()->pointer()->isConstrained() && !workspace()->globalShortcutsDisabled()) {
            wasAction = true;
            command = options->operationWindowMouseWheel(-1 * event->angleDelta().y());
        }
    } else {
        if (action == MouseAction::ModifierAndWindow) {
            command = c->getWheelCommand(Qt::Vertical, &wasAction);
        }
    }
    if (wasAction) {
        return std::make_pair(wasAction, !c->performMouseCommand(command, event->globalPos()));
    }
    return std::make_pair(wasAction, false);
}

}

class InternalWindowEventFilter : public InputEventFilter {
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalClient *>(input()->pointer()->focus())->internalWindow();
        QMouseEvent mouseEvent(event->type(),
                               event->pos() - internal->position(),
                               event->globalPos(),
                               event->button(), event->buttons(), event->modifiers());
        QCoreApplication::sendEvent(internal, &mouseEvent);
        return mouseEvent.isAccepted();
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalClient *>(input()->pointer()->focus())->internalWindow();
        const QPointF localPos = event->globalPosF() - internal->position();
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent wheelEvent(localPos, event->globalPosF(), QPoint(),
                               event->angleDelta() * -1,
                               delta * -1,
                               orientation,
                               event->buttons(),
                               event->modifiers());
        QCoreApplication::sendEvent(internal, &wheelEvent);
        return wheelEvent.isAccepted();
    }
    bool keyEvent(QKeyEvent *event) override {
        const QList<InternalClient *> &clients = workspace()->internalClients();
        QWindow *found = nullptr;
        for (auto it = clients.crbegin(); it != clients.crend(); ++it) {
            if (QWindow *w = (*it)->internalWindow()) {
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
        Qt::Key key = xkb->toQtKey( xkb->toKeysym(event->nativeScanCode()),
                                    event->nativeScanCode(),
                                    Qt::KeyboardModifiers(),
                                    true /* workaround for QTBUG-62102 */ );
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

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
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
        QWindow *internal = static_cast<InternalClient *>(input()->touch()->focus())->internalWindow();
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - internal->position();

        QEnterEvent enterEvent(m_lastLocalTouchPos, m_lastLocalTouchPos, pos);
        QCoreApplication::sendEvent(internal, &enterEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal, &e);
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
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
        QWindow *internal = static_cast<InternalClient *>(input()->touch()->focus())->internalWindow();
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());

        QMouseEvent e(QEvent::MouseMove, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        QCoreApplication::instance()->sendEvent(internal, &e);
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
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
        QWindow *internal = static_cast<InternalClient *>(input()->touch()->focus())->internalWindow();
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

class DecorationEventFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->globalPos() - decoration->client()->pos();
        switch (event->type()) {
        case QEvent::MouseMove: {
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->client()->processDecorationMove(p.toPoint(), event->globalPos());
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            const auto actionResult = performClientMouseAction(event, decoration->client());
            if (actionResult.first) {
                return actionResult.second;
            }
            QMouseEvent e(event->type(), p, event->globalPos(), event->button(), event->buttons(), event->modifiers());
            e.setTimestamp(event->timestamp());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && event->type() == QEvent::MouseButtonPress) {
                decoration->client()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                decoration->client()->processDecorationButtonRelease(&e);
            }
            return true;
        }
        default:
            break;
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        if (event->angleDelta().y() != 0) {
            // client window action only on vertical scrolling
            const auto actionResult = performClientWheelAction(event, decoration->client());
            if (actionResult.first) {
                return actionResult.second;
            }
        }
        const QPointF localPos = event->globalPosF() - decoration->client()->pos();
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta(),
                        delta,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration, &e);
        if (e.isAccepted()) {
            return true;
        }
        if ((orientation == Qt::Vertical) && decoration->client()->titlebarPositionUnderMouse()) {
            decoration->client()->performMouseCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                        event->globalPosF().toPoint());
        }
        return true;
    }
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
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
        m_lastLocalTouchPos = pos - decoration->client()->pos();

        QHoverEvent hoverEvent(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::sendEvent(decoration->decoration(), &hoverEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (!e.isAccepted()) {
            decoration->client()->processDecorationButtonPress(&e);
        }
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
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
        m_lastLocalTouchPos = pos - decoration->client()->pos();

        QHoverEvent e(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->client()->processDecorationMove(m_lastLocalTouchPos.toPoint(), pos.toPoint());
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
        Q_UNUSED(time);
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
        decoration->client()->processDecorationButtonRelease(&e);

        QHoverEvent leaveEvent(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::sendEvent(decoration->decoration(), &leaveEvent);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setDecorationPressId(-1);
        return true;
    }
    bool tabletToolEvent(TabletEvent *event) override {
        auto decoration = input()->tablet()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->globalPos() - decoration->client()->pos();
        switch (event->type()) {
        case QEvent::TabletMove:
        case QEvent::TabletEnterProximity: {
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->client()->processDecorationMove(p.toPoint(), event->globalPos());
            break;
        }
        case QEvent::TabletPress:
        case QEvent::TabletRelease: {
            const bool isPressed = event->type() == QEvent::TabletPress;
            QMouseEvent e(isPressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                          p,
                          event->globalPos(),
                          Qt::LeftButton,
                          isPressed ? Qt::LeftButton : Qt::MouseButtons(),
                          input()->keyboardModifiers());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && isPressed) {
                decoration->client()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::TabletRelease) {
                decoration->client()->processDecorationButtonRelease(&e);
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
};

#ifdef KWIN_BUILD_TABBOX
class TabBoxInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 button) override {
        Q_UNUSED(button)
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        return TabBox::TabBox::self()->handleMouseEvent(event);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setFocusedKeyboardSurface(nullptr);
        input()->pointer()->setEnableConstraints(false);
        // pass the key event to the seat, so that it has a proper model of the currently hold keys
        // this is important for combinations like alt+shift to ensure that shift is not considered pressed
        passToWaylandServer(event);

        if (event->type() == QEvent::KeyPress) {
            TabBox::TabBox::self()->keyPress(event->modifiers() | event->key());
        } else if (static_cast<KeyEvent*>(event)->modifiersRelevantForGlobalShortcuts() == Qt::NoModifier) {
            TabBox::TabBox::self()->modifiersReleased();
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        return TabBox::TabBox::self()->handleWheelEvent(event);
    }
};
#endif

class ScreenEdgeInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        ScreenEdges::self()->isEntered(event);
        // always forward
        return false;
    }
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        // TODO: better check whether a touch sequence is in progress
        if (m_touchInProgress || waylandServer()->seat()->isTouchSequence()) {
            // cancel existing touch
            ScreenEdges::self()->gestureRecognizer()->cancelSwipeGesture();
            m_touchInProgress = false;
            m_id = 0;
            return false;
        }
        if (ScreenEdges::self()->gestureRecognizer()->startSwipeGesture(pos) > 0) {
            m_touchInProgress = true;
            m_id = id;
            m_lastPos = pos;
            return true;
        }
        return false;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        if (m_touchInProgress && m_id == id) {
            ScreenEdges::self()->gestureRecognizer()->updateSwipeGesture(QSizeF(pos.x() - m_lastPos.x(), pos.y() - m_lastPos.y()));
            m_lastPos = pos;
            return true;
        }
        return false;
    }
    bool touchUp(qint32 id, quint32 time) override {
        Q_UNUSED(time)
        if (m_touchInProgress && m_id == id) {
            ScreenEdges::self()->gestureRecognizer()->endSwipeGesture();
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
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (event->type() != QEvent::MouseButtonPress) {
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->focus());
        if (!c) {
            return false;
        }
        const auto actionResult = performClientMouseAction(event, c, MouseAction::ModifierAndWindow);
        if (actionResult.first) {
            return actionResult.second;
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() == 0) {
            // only actions on vertical scroll
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->focus());
        if (!c) {
            return false;
        }
        const auto actionResult = performClientWheelAction(event, c, MouseAction::ModifierAndWindow);
        if (actionResult.first) {
            return actionResult.second;
        }
        return false;
    }
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(id)
        Q_UNUSED(time)
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->touch()->focus());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = c->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !c->performMouseCommand(command, pos.toPoint());
        }
        return false;
    }
    bool tabletToolEvent(TabletEvent *event) override {
        if (event->type() != QEvent::TabletPress) {
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->tablet()->focus());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = c->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !c->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
};

class InputKeyboardFilter : public InputEventFilter
{
public:
    bool keyEvent(QKeyEvent *event) override
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
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove: {
            seat->notifyPointerMotion(event->globalPos());
            MouseEvent *e = static_cast<MouseEvent*>(event);
            if (e->delta() != QSizeF()) {
                seat->relativePointerMotion(e->delta(), e->deltaUnaccelerated(), e->timestampMicroseconds());
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
    bool wheelEvent(QWheelEvent *event) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        auto _event = static_cast<WheelEvent *>(event);
        seat->notifyPointerAxis(_event->orientation(), _event->delta(), _event->discreteDelta(),
                                kwinAxisSourceToKWaylandAxisSource(_event->axisSource()));
        seat->notifyPointerFrame();
        return true;
    }
    bool keyEvent(QKeyEvent *event) override {
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
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchDown(id, pos);
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchMotion(id, pos);
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->notifyTouchUp(id);
        return true;
    }
    bool pinchGestureBegin(int fingerCount, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerPinchGesture(fingerCount);
        return true;
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerPinchGesture(delta, scale, angleDelta);
        return true;
    }
    bool pinchGestureEnd(quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerPinchGesture();
        return true;
    }
    bool pinchGestureCancelled(quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerPinchGesture();
        return true;
    }

    bool swipeGestureBegin(int fingerCount, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerSwipeGesture(fingerCount);
        return true;
    }
    bool swipeGestureUpdate(const QSizeF &delta, quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerSwipeGesture(delta);
        return true;
    }
    bool swipeGestureEnd(quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerSwipeGesture();
        return true;
    }
    bool swipeGestureCancelled(quint32 time) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerSwipeGesture();
        return true;
    }
    bool holdGestureBegin(int fingerCount, quint32 time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerHoldGesture(fingerCount);
        return true;
    }
    bool holdGestureEnd(quint32 time) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerHoldGesture();
        return true;
    }
    bool holdGestureCancelled(quint32 time) override
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
    explicit SurfaceCursor(QObject *parent) : Cursor(parent)
    {}

    void updateCursorSurface(KWaylandServer::SurfaceInterface *surface, const QPoint &hotspot) {
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
            updateCursor({}, {});
            return;
        }

        QImage cursorImage;
        cursorImage = buffer->data().copy();
        cursorImage.setDevicePixelRatio(m_surface->bufferScale());
        updateCursor(cursorImage, m_hotspot);
    }

    QPointer<KWaylandServer::SurfaceInterface> m_surface;
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

    KWaylandServer::TabletToolV2Interface::Type getType(const KWin::TabletToolId &tabletToolId) {
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

        TabletToolV2Interface *tool = tabletSeat->addTool(getType(tabletToolId), tabletToolId.m_serialId, tabletToolId.m_uniqueId, ifaceCapabilities);

        const auto cursor = new SurfaceCursor(tool);
        Cursors::self()->addCursor(cursor);
        m_cursorByTool[tool] = cursor;

        connect(tool, &TabletToolV2Interface::cursorChanged, cursor, [cursor] (TabletCursorV2 *tcursor) {
            static const auto createDefaultCursor = [] {
                WaylandCursorImage defaultCursor;
                WaylandCursorImage::Image ret;
                defaultCursor.loadThemeCursor(CursorShape(Qt::CrossCursor), &ret);
                return ret;
            };
            if (!tcursor || tcursor->enteredSerial() == 0) {
                static const auto defaultCursor = createDefaultCursor();
                cursor->updateCursor(defaultCursor.image, defaultCursor.hotspot);
                return;
            }
            auto cursorSurface = tcursor->surface();
            if (!cursorSurface) {
                cursor->updateCursor({}, {});
                return;
            }

            cursor->updateCursorSurface(cursorSurface, tcursor->hotspot());
        });
        Q_EMIT cursor->cursorChanged();
        return tool;
    }

    bool tabletToolEvent(TabletEvent *event) override
    {
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

        Toplevel *toplevel = input()->findToplevel(event->globalPos());
        if (!toplevel || !toplevel->surface()) {
            return false;
        }

        KWaylandServer::SurfaceInterface *surface = toplevel->surface();
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || (tablet && !tablet->isSurfaceSupported(surface))) {
            return emulateTabletEvent(event);
        }

        switch (event->type()) {
        case QEvent::TabletMove: {
            const auto pos = toplevel->mapToLocal(event->globalPosF());
            tool->sendMotion(pos);
            m_cursorByTool[tool]->setPos(event->globalPos());
            break;
        } case QEvent::TabletEnterProximity: {
            const QPoint pos = event->globalPos();
            m_cursorByTool[tool]->setPos(pos);
            tool->sendProximityIn(tablet);
            tool->sendMotion(toplevel->mapToLocal(event->globalPosF()));
            break;
        } case QEvent::TabletLeaveProximity:
            tool->sendProximityOut();
            break;
        case QEvent::TabletPress: {
            const auto pos = toplevel->mapToLocal(event->globalPosF());
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
        tool->sendPressure(MAX_VAL * event->pressure());
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
            input()->pointer()->processMotionAbsolute(event->globalPosF(), event->timestamp());
            break;
        case QEvent::TabletPress:
            input()->pointer()->processButton(KWin::qtMouseButtonToButton(Qt::LeftButton),
                                              InputRedirection::PointerButtonPressed, event->timestamp());
            break;
        case QEvent::TabletRelease:
            input()->pointer()->processButton(KWin::qtMouseButtonToButton(Qt::LeftButton),
                                              InputRedirection::PointerButtonReleased, event->timestamp());
            break;
        case QEvent::TabletLeaveProximity:
            break;
        default:
            qCWarning(KWIN_CORE) << "Unexpected tablet event type" << event;
            break;
        }
        return true;
    }

    bool tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId) override
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
        Toplevel *toplevel = workspace()->activeClient();
        auto seat = findTabletSeat();
        if (!toplevel || !toplevel->surface() || !seat->isClientSupported(toplevel->surface()->client())) {
            return nullptr;
        }

        auto tablet = static_cast<KWaylandServer::TabletV2Interface *>(tabletPadId.data);
        KWaylandServer::SurfaceInterface *surface = toplevel->surface();
        auto pad = tablet->pad();
        if (!pad) {
            return nullptr;
        }
        pad->setCurrentSurface(surface, tablet);
        return pad;
    }

    bool tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId) override
    {
        auto pad = findAndAdoptPad(tabletPadId);
        if (!pad) {
            return false;
        }
        pad->sendButton(QDateTime::currentMSecsSinceEpoch(), button, pressed);
        return true;
    }

    bool tabletPadRingEvent(int number, int angle, bool isFinger, const TabletPadId &tabletPadId) override
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
        ring->sendFrame(QDateTime::currentMSecsSinceEpoch());
        return true;
    }

    bool tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId) override
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
        strip->sendFrame(QDateTime::currentMSecsSinceEpoch());
        return true;
    }

    QHash<KWaylandServer::TabletToolV2Interface*, Cursor*> m_cursorByTool;
};

static KWaylandServer::AbstractDropHandler *dropHandler(Toplevel *toplevel)
{
    auto surface = toplevel->surface();
    if (!surface) {
        return nullptr;
    }
    auto seat = waylandServer()->seat();
    auto dropTarget = seat->dropHandlerForSurface(surface);
    if (dropTarget) {return dropTarget;}

    if (qobject_cast<X11Client*>(toplevel) && xwayland()) {
        return xwayland()->xwlDropHandler();
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

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
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

            const auto eventPos = event->globalPos();
            // TODO: use InputDeviceHandler::at() here and check isClient()?
            Toplevel *t = input()->findManagedToplevel(eventPos);
            const auto dragTarget = qobject_cast<AbstractClient*>(t);
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

            if (auto *xwl = xwayland()) {
                const auto ret = xwl->dragMoveFilter(t, eventPos);
                if (ret == Xwl::DragEventReply::Ignore) {
                    return false;
                } else if (ret == Xwl::DragEventReply::Take) {
                    break;
                }
            }

            if (t) {
                // TODO: consider decorations
                if (t->surface() != seat->dragSurface()) {
                    seat->setDragTarget(dropHandler(t), t->surface(), t->inputTransformation());
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

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override {
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
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
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

        if (Toplevel *t = input()->findToplevel(pos.toPoint())) {
            // TODO: consider decorations
            if (t->surface() != seat->dragSurface()) {
                if ((m_dragTarget = qobject_cast<AbstractClient*>(t))) {
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
    bool touchUp(qint32 id, quint32 time) override {
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
private:
    void raiseDragTarget()
    {
        m_raiseTimer.stop();
        if (m_dragTarget) {
            workspace()->takeActivity(m_dragTarget, Workspace::ActivityFlag::ActivityRaise);
        }
    }
    qint32 m_touchId = -1;
    QPointF m_lastPos = QPointF(-1, -1);
    QPointer<AbstractClient> m_dragTarget;
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
    qDeleteAll(m_inputBackends);
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
    if (waylandServer()) {
        m_keyboard->init();
        m_pointer->init();
        m_touch->init();
        m_tablet->init();

        updateLeds(m_keyboard->xkb()->leds());
        connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, this, &InputRedirection::updateLeds);

        setupTouchpadShortcuts();
        setupInputFilters();
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

class UserActivitySpy : public InputEventSpy
{
public:
    void pointerEvent(MouseEvent *event) override
    {
        Q_UNUSED(event)
        notifyActivity();
    }
    void wheelEvent(WheelEvent *event) override
    {
        Q_UNUSED(event)
        notifyActivity();
    }

    void keyEvent(KeyEvent *event) override
    {
        Q_UNUSED(event)
        notifyActivity();
    }

    void touchDown(qint32 id, const QPointF &pos, quint32 time) override
    {
        Q_UNUSED(id)
        Q_UNUSED(pos)
        Q_UNUSED(time)
        notifyActivity();
    }
    void touchMotion(qint32 id, const QPointF &pos, quint32 time) override
    {
        Q_UNUSED(id)
        Q_UNUSED(pos)
        Q_UNUSED(time)
        notifyActivity();
    }
    void touchUp(qint32 id, quint32 time) override
    {
        Q_UNUSED(id)
        Q_UNUSED(time)
        notifyActivity();
    }

    void pinchGestureBegin(int fingerCount, quint32 time) override
    {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        notifyActivity();
    }
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override
    {
        Q_UNUSED(scale)
        Q_UNUSED(angleDelta)
        Q_UNUSED(delta)
        Q_UNUSED(time)
        notifyActivity();
    }
    void pinchGestureEnd(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }
    void pinchGestureCancelled(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }

    void swipeGestureBegin(int fingerCount, quint32 time) override
    {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        notifyActivity();
    }
    void swipeGestureUpdate(const QSizeF &delta, quint32 time) override
    {
        Q_UNUSED(delta)
        Q_UNUSED(time)
        notifyActivity();
    }
    void swipeGestureEnd(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }
    void swipeGestureCancelled(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }

    void holdGestureBegin(int fingerCount, quint32 time) override
    {
        Q_UNUSED(fingerCount)
        Q_UNUSED(time)
        notifyActivity();
    }
    void holdGestureEnd(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }
    void holdGestureCancelled(quint32 time) override
    {
        Q_UNUSED(time)
        notifyActivity();
    }

    void tabletToolEvent(TabletEvent *event) override
    {
        Q_UNUSED(event)
        notifyActivity();
    }
    void tabletToolButtonEvent(uint button, bool pressed, const TabletToolId &tabletToolId) override
    {
        Q_UNUSED(button)
        Q_UNUSED(pressed)
        Q_UNUSED(tabletToolId)
        notifyActivity();
    }
    void tabletPadButtonEvent(uint button, bool pressed, const TabletPadId &tabletPadId) override
    {
        Q_UNUSED(button)
        Q_UNUSED(pressed)
        Q_UNUSED(tabletPadId)
        notifyActivity();
    }
    void tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId) override
    {
        Q_UNUSED(number)
        Q_UNUSED(position)
        Q_UNUSED(isFinger)
        Q_UNUSED(tabletPadId)
        notifyActivity();
    }
    void tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId) override
    {
        Q_UNUSED(number)
        Q_UNUSED(position)
        Q_UNUSED(isFinger)
        Q_UNUSED(tabletPadId)
        notifyActivity();
    }

private:
    void notifyActivity()
    {
        waylandServer()->simulateUserActivity();
    }
};

void InputRedirection::setupInputFilters()
{
    const bool hasGlobalShortcutSupport = waylandServer()->hasGlobalShortcutSupport();
    if ((kwinApp()->platform()->session()->capabilities() & Session::Capability::SwitchTerminal)
            && hasGlobalShortcutSupport) {
        installInputEventFilter(new VirtualTerminalFilter);
    }
    installInputEventSpy(new HideCursorSpy);
    installInputEventSpy(new UserActivitySpy);
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
    installInputEventFilter(new EffectsFilter);
    installInputEventFilter(new MoveResizeFilter);
#ifdef KWIN_BUILD_TABBOX
    installInputEventFilter(new TabBoxInputFilter);
#endif
    if (hasGlobalShortcutSupport) {
        installInputEventFilter(new GlobalShortcutFilter);
    }
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

        for (InputDevice *device : qAsConst(m_inputDevices)) {
            device->setLeds(leds);
        }
    }
}

void InputRedirection::handleInputDeviceAdded(InputDevice *device)
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

    auto handleSwitchEvent = [this] (SwitchEvent::State state, quint32 time, quint64 timeMicroseconds, InputDevice *device) {
        SwitchEvent event(state, time, timeMicroseconds, device);
        processSpies(std::bind(&InputEventSpy::switchEvent, std::placeholders::_1, &event));
        processFilters(std::bind(&InputEventFilter::switchEvent, std::placeholders::_1, &event));
    };
    connect(device, &InputDevice::switchToggledOn, this,
            std::bind(handleSwitchEvent, SwitchEvent::State::On, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    connect(device, &InputDevice::switchToggledOff, this,
            std::bind(handleSwitchEvent, SwitchEvent::State::Off, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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

void InputRedirection::handleInputDeviceRemoved(InputDevice *device)
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
    for (InputDevice *device : qAsConst(m_inputDevices)) {
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
                                                          QStringLiteral("touchpadEnabledChanged")
        );
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

void InputRedirection::addInputBackend(InputBackend *inputBackend)
{
    Q_ASSERT(!m_inputBackends.contains(inputBackend));
    m_inputBackends.append(inputBackend);

    connect(inputBackend, &InputBackend::deviceAdded, this, &InputRedirection::handleInputDeviceAdded);
    connect(inputBackend, &InputBackend::deviceRemoved, this, &InputRedirection::handleInputDeviceRemoved);

    inputBackend->setConfig(InputConfig::self()->inputConfig());
    inputBackend->initialize();
}

void InputRedirection::setupInputBackends()
{
    InputBackend *inputBackend = kwinApp()->platform()->createInputBackend();
    if (inputBackend) {
        addInputBackend(inputBackend);
    }
    if (waylandServer()) {
        addInputBackend(new FakeInputBackend());
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
    KGlobalAccel::self()->setDefaultShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle});
    KGlobalAccel::self()->setShortcut(touchpadToggleAction, QList<QKeySequence>{Qt::Key_TouchpadToggle});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setShortcut(touchpadOnAction, QList<QKeySequence>{Qt::Key_TouchpadOn});
    KGlobalAccel::self()->setDefaultShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
    KGlobalAccel::self()->setShortcut(touchpadOffAction, QList<QKeySequence>{Qt::Key_TouchpadOff});
#ifndef KWIN_BUILD_TESTING
    registerShortcut(Qt::Key_TouchpadToggle, touchpadToggleAction);
    registerShortcut(Qt::Key_TouchpadOn, touchpadOnAction);
    registerShortcut(Qt::Key_TouchpadOff, touchpadOffAction);
#endif
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

Toplevel *InputRedirection::findToplevel(const QPoint &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
    // TODO: check whether the unmanaged wants input events at all
    if (!isScreenLocked) {
        // if an effect overrides the cursor we don't have a window to focus
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->isMouseInterception()) {
            return nullptr;
        }
        const QList<Unmanaged *> &unmanaged = Workspace::self()->unmanagedList();
        for (Unmanaged *u : unmanaged) {
            if (u->hitTest(pos)) {
                return u;
            }
        }
    }
    return findManagedToplevel(pos);
}

Toplevel *InputRedirection::findManagedToplevel(const QPoint &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
    const QList<Toplevel *> &stacking = Workspace::self()->stackingOrder();
    if (stacking.isEmpty()) {
        return nullptr;
    }
    auto it = stacking.end();
    do {
        --it;
        Toplevel *t = (*it);
        if (t->isDeleted()) {
            // a deleted window doesn't get mouse events
            continue;
        }
        if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
            if (!c->isOnCurrentActivity() || !c->isOnCurrentDesktop() || c->isMinimized() || c->isHiddenInternal()) {
                continue;
            }
        }
        if (!t->readyForPainting()) {
            continue;
        }
        if (isScreenLocked) {
            if (!t->isLockScreen() && !t->isInputMethod()) {
                continue;
            }
        }
        if (t->hitTest(pos)) {
            return t;
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

void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action)
{
    Q_UNUSED(shortcut)
    kwinApp()->platform()->setupActionForGlobalAccel(action);
}

void InputRedirection::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
}

void InputRedirection::registerRealtimeTouchpadSwipeShortcut(SwipeDirection direction, QAction *action, std::function<void(qreal)> cb)
{
    m_shortcuts->registerRealtimeTouchpadSwipe(action, cb, direction);
}

void InputRedirection::registerTouchpadSwipeShortcut(SwipeDirection direction, QAction *action)
{
    m_shortcuts->registerTouchpadSwipe(action, direction);
}

void InputRedirection::registerGlobalAccel(KGlobalAccelInterface *interface)
{
    m_shortcuts->setKGlobalAccelInterface(interface);
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

void InputRedirection::startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
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
    connect(workspace(), &Workspace::clientMinimizedChanged, this, &InputDeviceHandler::update);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, this, &InputDeviceHandler::update);
}

bool InputDeviceHandler::setHover(Toplevel *toplevel)
{
    if (m_hover.window == toplevel) {
        return false;
    }
    auto old = m_hover.window;
    disconnect(m_hover.surfaceCreatedConnection);
    m_hover.surfaceCreatedConnection = QMetaObject::Connection();

    m_hover.window = toplevel;
    return true;
}

void InputDeviceHandler::setFocus(Toplevel *toplevel)
{
    if (m_focus.window != toplevel) {
        Toplevel *oldFocus = m_focus.window;
        m_focus.window = toplevel;
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
    Toplevel *focus = m_hover.window;

    if (m_focus.decoration) {
        focus = nullptr;
    } else if (m_hover.window && !m_hover.window->surface() && !m_hover.window->isInternal()) {
        // The surface has not yet been created (special XWayland case).
        // Therefore listen for its creation.
        if (!m_hover.surfaceCreatedConnection) {
            m_hover.surfaceCreatedConnection = connect(m_hover.window, &Toplevel::surfaceChanged,
                                                    this, &InputDeviceHandler::update);
        }
        focus = nullptr;
    }

    setFocus(focus);
}

void InputDeviceHandler::updateDecoration()
{
    Decoration::DecoratedClientImpl *decoration = nullptr;
    auto *ac = qobject_cast<AbstractClient*>(m_hover.window);
    if (ac && ac->decoratedClient()) {
        if (!ac->clientGeometry().contains(position().toPoint())) {
            // input device above decoration
            decoration = ac->decoratedClient();
        }
    }

    setDecoration(decoration);
}

void InputDeviceHandler::update()
{
    if (!m_inited) {
        return;
    }

    Toplevel *toplevel = nullptr;
    if (positionValid()) {
        toplevel = input()->findToplevel(position().toPoint());
    }
    // Always set the toplevel at the position of the input device.
    setHover(toplevel);

    if (focusUpdatesBlocked()) {
        workspace()->updateFocusMousePosition(position().toPoint());
        return;
    }

    updateDecoration();
    updateFocus();

    workspace()->updateFocusMousePosition(position().toPoint());
}

Toplevel *InputDeviceHandler::hover() const
{
    return m_hover.window.data();
}

Toplevel *InputDeviceHandler::focus() const
{
    return m_focus.window.data();
}

Decoration::DecoratedClientImpl *InputDeviceHandler::decoration() const
{
    return m_focus.decoration;
}

} // namespace

#include "input.moc"
