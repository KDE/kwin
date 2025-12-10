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
#include "core/inputbackend.h"
#include "core/inputdevice.h"
#include "core/session.h"
#include "effect/effecthandler.h"
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
#include "wayland/abstract_data_source.h"
#include "wayland/xdgtopleveldrag_v1.h"
#if KWIN_BUILD_X11
#include "x11window.h"
#endif
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
#include "screenedgegestures.h"
#include "virtualdesktops.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/tablet_v2.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xdgactivationv1.h"
#include "xkb.h"
#include "xwayland/xwayland_interface.h"

#include <KDecoration3/Decoration>
#include <KLocalizedString>
#include <decorations/decoratedwindow.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif
// Qt
#include <QAction>
#include <QKeyEvent>
#include <QThread>
#include <qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon.h>

#include "osd.h"
#include "wayland/xdgshell.h"
#include <cmath>
#include <linux/input.h>

using namespace std::literals;

namespace KWin
{

InputEventFilter::InputEventFilter(InputFilterOrder::Order weight)
    : m_weight(weight)
{
}

InputEventFilter::~InputEventFilter()
{
    if (input()) {
        input()->uninstallInputEventFilter(this);
    }
}

int InputEventFilter::weight() const
{
    return m_weight;
}

bool InputEventFilter::pointerMotion(PointerMotionEvent *event)
{
    return false;
}

bool InputEventFilter::pointerButton(PointerButtonEvent *event)
{
    return false;
}

bool InputEventFilter::pointerFrame()
{
    return false;
}

bool InputEventFilter::pointerAxis(PointerAxisEvent *event)
{
    return false;
}

bool InputEventFilter::keyboardKey(KeyboardKeyEvent *event)
{
    return false;
}

bool InputEventFilter::touchDown(TouchDownEvent *event)
{
    return false;
}

bool InputEventFilter::touchMotion(TouchMotionEvent *event)
{
    return false;
}

bool InputEventFilter::touchUp(TouchUpEvent *event)
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

bool InputEventFilter::pinchGestureBegin(PointerPinchGestureBeginEvent *event)
{
    return false;
}

bool InputEventFilter::pinchGestureUpdate(PointerPinchGestureUpdateEvent *event)
{
    return false;
}

bool InputEventFilter::pinchGestureEnd(PointerPinchGestureEndEvent *event)
{
    return false;
}

bool InputEventFilter::pinchGestureCancelled(PointerPinchGestureCancelEvent *event)
{
    return false;
}

bool InputEventFilter::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
    return false;
}

bool InputEventFilter::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
    return false;
}

bool InputEventFilter::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
    return false;
}

bool InputEventFilter::swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
{
    return false;
}

bool InputEventFilter::holdGestureBegin(PointerHoldGestureBeginEvent *event)
{
    return false;
}

bool InputEventFilter::holdGestureEnd(PointerHoldGestureEndEvent *event)
{
    return false;
}

bool InputEventFilter::holdGestureCancelled(PointerHoldGestureCancelEvent *event)
{
    return false;
}

bool InputEventFilter::switchEvent(SwitchEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolProximityEvent(TabletToolProximityEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolAxisEvent(TabletToolAxisEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolTipEvent(TabletToolTipEvent *event)
{
    return false;
}

bool InputEventFilter::tabletToolButtonEvent(TabletToolButtonEvent *event)
{
    return false;
}

bool InputEventFilter::tabletPadButtonEvent(TabletPadButtonEvent *event)
{
    return false;
}

bool InputEventFilter::tabletPadStripEvent(TabletPadStripEvent *event)
{
    return false;
}

bool InputEventFilter::tabletPadRingEvent(TabletPadRingEvent *event)
{
    return false;
}

bool InputEventFilter::tabletPadDialEvent(TabletPadDialEvent *event)
{
    return false;
}

bool InputEventFilter::passToInputMethod(KeyboardKeyEvent *event)
{
    static QStringList s_deviceSkipsInputMethods = qEnvironmentVariable("KWIN_DEVICE_SKIPS_INPUT_METHOD").split(',');
    if (!kwinApp()->inputMethod()) {
        return false;
    }
    if (event->device && s_deviceSkipsInputMethods.contains(event->device->name())) {
        return false;
    }
    if (auto keyboardGrab = kwinApp()->inputMethod()->keyboardGrab()) {
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp);
        keyboardGrab->sendKey(waylandServer()->display()->nextSerial(), std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count(), event->nativeScanCode, event->state);
        return true;
    } else {
        kwinApp()->inputMethod()->commitPendingText();
        return false;
    }
}

class VirtualTerminalFilter : public InputEventFilter
{
public:
    VirtualTerminalFilter()
        : InputEventFilter(InputFilterOrder::VirtualTerminal)
    {
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        // really on press and not on release? X11 switches on press.
        if (event->state == KeyboardKeyState::Pressed) {
            const xkb_keysym_t keysym = event->nativeVirtualKey;
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                kwinApp()->session()->switchTo(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
                return true;
            }
        }
        return false;
    }
};

#if KWIN_BUILD_SCREENLOCKER
class LockScreenFilter : public InputEventFilter
{
public:
    LockScreenFilter()
        : InputEventFilter(InputFilterOrder::LockScreen)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        auto window = input()->findToplevel(event->position);
        if (window && window->isClient() && window->isLockScreen()) {
            workspace()->activateWindow(window);
        }

        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            // TODO: should the pointer position always stay in sync, i.e. not do the check?
            seat->setTimestamp(event->timestamp);
            seat->notifyPointerMotion(event->position);
        }
        return true;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        auto window = input()->findToplevel(event->position);
        if (window && window->isClient() && window->isLockScreen()) {
            workspace()->activateWindow(window);
        }

        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            // TODO: can we leak presses/releases here when we move the mouse in between from an allowed surface to
            //       disallowed one or vice versa?
            seat->setTimestamp(event->timestamp);
            seat->notifyPointerButton(event->nativeButton, event->state);
        }
        return true;
    }
    bool pointerFrame() override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            seat->notifyPointerFrame();
        }
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            seat->setTimestamp(event->timestamp);
            seat->notifyPointerAxis(event->orientation, event->delta, event->deltaV120, event->source, event->inverted);
        }
        return true;
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        // FIXME: Ideally we want to move all whitelisted global shortcuts here and process it here instead of lockscreen
        if (event->key == Qt::Key_PowerOff) {
            // globalshortcuts want to use this
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        // send event to KSldApp for global accel
        // if event is set to accepted it means a whitelisted shortcut was triggered
        // in that case we filter it out and don't process it further
        QKeyEvent keyEvent(event->state == KeyboardKeyState::Released ? QEvent::KeyRelease : QEvent::KeyPress,
                           event->key,
                           event->modifiers,
                           event->nativeScanCode,
                           event->nativeVirtualKey,
                           0,
                           event->text,
                           event->state == KeyboardKeyState::Repeated);
        keyEvent.setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), &keyEvent);
        if (keyEvent.isAccepted()) {
            return true;
        }

        // continue normal processing
        input()->keyboard()->update();
        if (!keyboardSurfaceAllowed()) {
            // don't pass event to seat
            return true;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp);
        seat->notifyKeyboardKey(event->nativeScanCode, event->state, event->serial);
        return true;
    }
    bool touchDown(TouchDownEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        Window *window = input()->findToplevel(event->pos);
        if (window && surfaceAllowed(window->surface())) {
            auto seat = waylandServer()->seat();
            seat->setTimestamp(event->time);
            seat->notifyTouchDown(window->surface(), window->bufferGeometry().topLeft(), event->id, event->pos);
        }
        return true;
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->notifyTouchMotion(event->id, event->pos);
        return true;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }

        ScreenLocker::KSldApp::self()->userActivity();

        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->notifyTouchUp(event->id);
        return true;
    }
    bool pinchGestureBegin(PointerPinchGestureBeginEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureEnd(PointerPinchGestureEndEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureBegin(PointerHoldGestureBeginEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureEnd(PointerHoldGestureEndEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }
    bool holdGestureCancelled(PointerHoldGestureCancelEvent *event) override
    {
        // no touchpad multi-finger gestures on lock screen
        return waylandServer()->isScreenLocked();
    }

private:
    bool surfaceAllowed(SurfaceInterface *s) const
    {
        if (s) {
            if (Window *t = waylandServer()->findWindow(s)) {
                return t->isLockScreen() || t->isInputMethod() || t->isLockScreenOverlay();
            }
            return false;
        }
        return true;
    }
    bool pointerSurfaceAllowed() const
    {
        return surfaceAllowed(waylandServer()->seat()->focusedPointerSurface());
    }
    bool keyboardSurfaceAllowed() const
    {
        return surfaceAllowed(waylandServer()->seat()->focusedKeyboardSurface());
    }
};
#endif

class EffectsFilter : public InputEventFilter
{
public:
    EffectsFilter()
        : InputEventFilter(InputFilterOrder::Effects)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        if (!effects) {
            return false;
        }
        QMouseEvent mouseEvent(QEvent::MouseMove,
                               event->position,
                               event->position,
                               Qt::NoButton,
                               event->buttons,
                               event->modifiers);
        mouseEvent.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        mouseEvent.setAccepted(false);
        return effects->checkInputWindowEvent(&mouseEvent);
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!effects) {
            return false;
        }
        QMouseEvent mouseEvent(event->state == PointerButtonState::Pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                               event->position,
                               event->position,
                               event->button,
                               event->buttons,
                               event->modifiers);
        mouseEvent.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        mouseEvent.setAccepted(false);
        return effects->checkInputWindowEvent(&mouseEvent);
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (!effects) {
            return false;
        }
        QWheelEvent wheelEvent(event->position,
                               event->position,
                               QPoint(),
                               (event->orientation == Qt::Horizontal) ? QPoint(event->delta, 0) : QPoint(0, event->delta),
                               event->buttons,
                               event->modifiers,
                               Qt::NoScrollPhase,
                               event->inverted);
        wheelEvent.setAccepted(false);
        return effects->checkInputWindowEvent(&wheelEvent);
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (!effects || !effects->hasKeyboardGrab()) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        if (!passToInputMethod(event)) {
            QKeyEvent keyEvent(event->state == KeyboardKeyState::Released ? QEvent::KeyRelease : QEvent::KeyPress,
                               event->key,
                               event->modifiers,
                               event->nativeScanCode,
                               event->nativeVirtualKey,
                               0,
                               event->text,
                               event->state == KeyboardKeyState::Repeated);
            keyEvent.setAccepted(false);
            effects->grabbedKeyboardEvent(&keyEvent);
        }
        return true;
    }
    bool touchDown(TouchDownEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->touchDown(event->id, event->pos, event->time);
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->touchMotion(event->id, event->pos, event->time);
    }
    bool touchUp(TouchUpEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->touchUp(event->id, event->time);
    }
    bool touchCancel() override
    {
        effects->touchCancel();
        return false;
    }
    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletToolProximityEvent(event);
    }
    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletToolAxisEvent(event);
    }
    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletToolTipEvent(event);
    }
    bool tabletToolButtonEvent(TabletToolButtonEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletToolButtonEvent(event->button, event->pressed, event->tool, event->time);
    }
    bool tabletPadButtonEvent(TabletPadButtonEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletPadButtonEvent(event->button, event->pressed, event->time, event->device);
    }
    bool tabletPadStripEvent(TabletPadStripEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletPadStripEvent(event->number, event->position, event->isFinger, event->time, event->device);
    }
    bool tabletPadRingEvent(TabletPadRingEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletPadRingEvent(event->number, event->position, event->isFinger, event->time, event->device);
    }
    bool tabletPadDialEvent(TabletPadDialEvent *event) override
    {
        if (!effects) {
            return false;
        }
        return effects->tabletPadDialEvent(event->number, event->delta, event->time, event->device);
    }
};

class MoveResizeFilter : public InputEventFilter
{
public:
    MoveResizeFilter()
        : InputEventFilter(InputFilterOrder::InteractiveMoveResize)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        window->updateInteractiveMoveResize(event->position, event->modifiers);
        return true;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (event->state == PointerButtonState::Released) {
            if (event->buttons == Qt::NoButton) {
                window->endInteractiveMoveResize();
            }
        }
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        // filter out while moving a window
        return workspace()->moveResizeWindow() != nullptr;
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (event->state == KeyboardKeyState::Repeated || event->state == KeyboardKeyState::Pressed) {
            window->keyPressEvent(QKeyCombination{event->modifiers, event->key});
        }
        if (window->isInteractiveMove() || window->isInteractiveResize()) {
            // only update if mode didn't end
            window->updateInteractiveMoveResize(input()->globalPointer(), input()->keyboardModifiers());
        }
        return true;
    }

    bool touchDown(TouchDownEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        return true;
    }

    bool touchMotion(TouchMotionEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (!m_set) {
            m_id = event->id;
            m_set = true;
        }
        if (m_id == event->id) {
            window->updateInteractiveMoveResize(event->pos, input()->keyboardModifiers());
        }
        return true;
    }

    bool touchUp(TouchUpEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (m_id == event->id || !m_set) {
            window->endInteractiveMoveResize();
            m_set = false;
            // pass through to update decoration filter later on
            return false;
        }
        m_set = false;
        return true;
    }

    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }

        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }
        if (event->type == TabletToolTipEvent::Release) {
            window->endInteractiveMoveResize();
        }
        return true;
    }

    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        Window *window = workspace()->moveResizeWindow();
        if (!window) {
            return false;
        }

        window->updateInteractiveMoveResize(event->position, input()->keyboardModifiers());
        return true;
    }

private:
    qint32 m_id = 0;
    bool m_set = false;
};

class WindowSelectorFilter : public InputEventFilter
{
public:
    WindowSelectorFilter()
        : InputEventFilter(InputFilterOrder::WindowSelector)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        return m_active;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!m_active) {
            return false;
        }
        if (event->state == PointerButtonState::Released) {
            if (event->buttons == Qt::NoButton) {
                if (event->button == Qt::RightButton) {
                    cancel();
                } else {
                    accept(event->position);
                }
            }
        }
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        // filter out while selecting a window
        return m_active;
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (!m_active) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);

        if (event->state == KeyboardKeyState::Repeated || event->state == KeyboardKeyState::Pressed) {
            // x11 variant does this on key press, so do the same
            if (event->key == Qt::Key_Escape) {
                cancel();
            } else if (event->key == Qt::Key_Enter || event->key == Qt::Key_Return || event->key == Qt::Key_Space) {
                accept(input()->globalPointer());
            }
            if (input()->supportsPointerWarping()) {
                int mx = 0;
                int my = 0;
                if (event->key == Qt::Key_Left) {
                    mx = -10;
                }
                if (event->key == Qt::Key_Right) {
                    mx = 10;
                }
                if (event->key == Qt::Key_Up) {
                    my = -10;
                }
                if (event->key == Qt::Key_Down) {
                    my = 10;
                }
                if (event->modifiers & Qt::ControlModifier) {
                    mx /= 10;
                    my /= 10;
                }
                input()->warpPointer(input()->globalPointer() + QPointF(mx, my));
            }
        }
        // filter out while selecting a window
        return true;
    }

    bool touchDown(TouchDownEvent *event) override
    {
        if (!isActive()) {
            return false;
        }
        m_touchPoints.insert(event->id, event->pos);
        return true;
    }

    bool touchMotion(TouchMotionEvent *event) override
    {
        if (!isActive()) {
            return false;
        }
        auto it = m_touchPoints.find(event->id);
        if (it != m_touchPoints.end()) {
            *it = event->pos;
        }
        return true;
    }

    bool touchUp(TouchUpEvent *event) override
    {
        if (!isActive()) {
            return false;
        }
        auto it = m_touchPoints.find(event->id);
        if (it != m_touchPoints.end()) {
            const auto pos = it.value();
            m_touchPoints.erase(it);
            if (m_touchPoints.isEmpty()) {
                accept(pos);
            }
        }
        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        if (!isActive()) {
            return false;
        }

        if (event->type == TabletToolTipEvent::Release) {
            accept(event->position);
        }

        return true;
    }

    bool isActive() const
    {
        return m_active;
    }
    void start(std::function<void(Window *)> callback)
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
        m_callback = std::function<void(Window *)>();
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
    std::function<void(Window *)> m_callback;
    std::function<void(const QPoint &)> m_pointSelectionFallback;
    QMap<quint32, QPointF> m_touchPoints;
};

class MouseWheelAccumulator
{
public:
    qreal accumulate(PointerAxisEvent *event)
    {
        const qreal delta = event->deltaV120 != 0 ? event->deltaV120 / 120.0 : event->delta / 15.0;
        if (std::signbit(m_scrollDistance) != std::signbit(delta)) {
            m_scrollDistance = 0;
        }
        m_scrollDistance += delta;
        if (std::abs(m_scrollDistance) >= 1.0) {
            const qreal ret = m_scrollDistance;
            m_scrollDistance = std::fmod(m_scrollDistance, 1.0f);
            return ret - m_scrollDistance;
        } else {
            return 0;
        }
    }

private:
    qreal m_scrollDistance = 0;
};

#if KWIN_BUILD_GLOBALSHORTCUTS
class GlobalShortcutFilter : public InputEventFilter
{
public:
    GlobalShortcutFilter()
        : InputEventFilter(InputFilterOrder::GlobalShortcut)
    {
        m_powerDown.setSingleShot(true);
        m_powerDown.setInterval(1000);
    }

    bool pointerButton(PointerButtonEvent *event) override
    {
        if (event->state == PointerButtonState::Pressed) {
            if (input()->shortcuts()->processPointerPressed(event->modifiers, event->buttons)) {
                return true;
            }
        }
        return false;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (event->modifiers == Qt::NoModifier) {
            return false;
        }
        PointerAxisDirection direction = PointerAxisUp;
        if (event->orientation == Qt::Horizontal) {
            if (event->delta > 0) {
                direction = PointerAxisRight;
            } else if (event->delta < 0) {
                direction = PointerAxisLeft;
            }
            return input()->shortcuts()->processAxis(event->modifiers, direction, m_horizontalAccumulator.accumulate(event));
        } else {
            if (event->delta > 0) {
                direction = PointerAxisDown;
            } else if (event->delta < 0) {
                direction = PointerAxisUp;
            }
            return input()->shortcuts()->processAxis(event->modifiers, direction, m_verticalAccumulator.accumulate(event));
        }
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->key == Qt::Key_PowerOff) {
            const auto modifiers = event->modifiersRelevantForGlobalShortcuts;
            if (event->state == KeyboardKeyState::Pressed) {
                auto passToShortcuts = [modifiers] {
                    input()->shortcuts()->processKey(modifiers, Qt::Key_PowerDown, KeyboardKeyState::Pressed);
                };
                QObject::connect(&m_powerDown, &QTimer::timeout, input()->shortcuts(), passToShortcuts, Qt::SingleShotConnection);
                m_powerDown.start();
                return true;
            } else if (event->state == KeyboardKeyState::Released) {
                if (m_powerDown.isActive()) {
                    bool ret = input()->shortcuts()->processKey(modifiers, event->key, KeyboardKeyState::Pressed);
                    ret |= input()->shortcuts()->processKey(modifiers, event->key, KeyboardKeyState::Released);
                    m_powerDown.stop();
                    return ret;
                } else {
                    return input()->shortcuts()->processKey(modifiers, event->key, event->state);
                }
            }
        } else if (event->state == KeyboardKeyState::Repeated || event->state == KeyboardKeyState::Pressed) {
            if (!waylandServer()->isKeyboardShortcutsInhibited()) {
                if (input()->shortcuts()->processKey(event->modifiersRelevantForGlobalShortcuts, event->key, event->state)) {
                    input()->keyboard()->addFilteredKey(event->nativeScanCode);
                    return true;
                }
            }
        } else if (event->state == KeyboardKeyState::Released) {
            if (!waylandServer()->isKeyboardShortcutsInhibited()) {
                return input()->shortcuts()->processKey(event->modifiersRelevantForGlobalShortcuts, event->key, event->state);
            }
        }
        return false;
    }
    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        m_touchpadGestureFingerCount = event->fingerCount;
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeStart(DeviceType::Touchpad, event->fingerCount);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeUpdate(DeviceType::Touchpad, event->delta);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeCancel(DeviceType::Touchpad);
            return true;
        } else {
            return false;
        }
    }
    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processSwipeEnd(DeviceType::Touchpad);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureBegin(PointerPinchGestureBeginEvent *event) override
    {
        m_touchpadGestureFingerCount = event->fingerCount;
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchStart(event->fingerCount);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchUpdate(event->scale, event->angleDelta, event->delta);
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureEnd(PointerPinchGestureEndEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchEnd();
            return true;
        } else {
            return false;
        }
    }
    bool pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override
    {
        if (m_touchpadGestureFingerCount >= 3) {
            input()->shortcuts()->processPinchCancel();
            return true;
        } else {
            return false;
        }
    }
    bool touchDown(TouchDownEvent *event) override
    {
        if (m_gestureTaken) {
            input()->shortcuts()->processSwipeCancel(DeviceType::Touchscreen);
            m_gestureCancelled = true;
            return true;
        } else {
            if (m_touchPoints.isEmpty()) {
                m_lastTouchDownTime = event->time;
            } else {
                if (event->time - m_lastTouchDownTime > 250ms) {
                    m_gestureCancelled = true;
                    return false;
                }
                m_lastTouchDownTime = event->time;
                auto output = workspace()->outputAt(event->pos);
                auto physicalSize = output->orientateSize(output->physicalSize());
                if (!physicalSize.isValid()) {
                    physicalSize = QSize(190, 100);
                }
                float xfactor = physicalSize.width() / (float)output->geometry().width();
                float yfactor = physicalSize.height() / (float)output->geometry().height();
                bool distanceMatch = std::any_of(m_touchPoints.constBegin(), m_touchPoints.constEnd(), [event, xfactor, yfactor](const auto &point) {
                    QPointF p = event->pos - point;
                    return std::abs(xfactor * p.x()) + std::abs(yfactor * p.y()) < 50;
                });
                if (!distanceMatch) {
                    m_gestureCancelled = true;
                    return false;
                }
            }
            m_touchPoints.insert(event->id, event->pos);
            if (m_touchPoints.count() >= 3 && !m_gestureCancelled) {
                m_gestureTaken = true;
                m_syntheticCancel = true;
                input()->processFilters(&InputEventFilter::touchCancel);
                m_syntheticCancel = false;
                input()->shortcuts()->processSwipeStart(DeviceType::Touchscreen, m_touchPoints.count());
                return true;
            }
        }
        return false;
    }

    bool touchMotion(TouchMotionEvent *event) override
    {
        if (m_gestureTaken) {
            if (m_gestureCancelled) {
                return true;
            }
            auto output = workspace()->outputAt(event->pos);
            const auto physicalSize = output->orientateSize(output->physicalSize());
            const float xfactor = physicalSize.width() / (float)output->geometry().width();
            const float yfactor = physicalSize.height() / (float)output->geometry().height();

            auto &point = m_touchPoints[event->id];
            const QPointF dist = event->pos - point;
            const QPointF delta = QPointF(xfactor * dist.x(), yfactor * dist.y());
            input()->shortcuts()->processSwipeUpdate(DeviceType::Touchscreen, 5 * delta / m_touchPoints.size());
            point = event->pos;
            return true;
        }
        return false;
    }

    bool touchUp(TouchUpEvent *event) override
    {
        m_touchPoints.remove(event->id);
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
    MouseWheelAccumulator m_horizontalAccumulator;
    MouseWheelAccumulator m_verticalAccumulator;

    QTimer m_powerDown;
};
#endif

namespace
{

static std::optional<Options::MouseCommand> globalWindowAction(Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    if (modifiers != options->commandAllModifier()) {
        return std::nullopt;
    }
    if (workspace()->globalShortcutsDisabled()) {
        return std::nullopt;
    }
    switch (button) {
    case Qt::LeftButton:
        return options->commandAll1();
    case Qt::MiddleButton:
        return options->commandAll2();
    case Qt::RightButton:
        return options->commandAll3();
    default:
        return std::nullopt;
    }
}

static std::optional<Options::MouseCommand> windowActionForPointerButtonPress(PointerButtonEvent *event, Window *window)
{
    if (!input()->pointer()->isConstrained()) {
        if (const auto command = globalWindowAction(event->button, event->modifiersRelevantForShortcuts)) {
            return command;
        }
    }

    return window->getMousePressCommand(event->button);
}

static std::optional<Options::MouseCommand> windowActionForTouchDown(Window *window)
{
    if (const auto command = globalWindowAction(Qt::LeftButton, input()->modifiersRelevantForGlobalShortcuts())) {
        return command;
    } else {
        return window->getMousePressCommand(Qt::LeftButton);
    }
}

static std::optional<Options::MouseCommand> windowActionForTabletTipDown(Window *window)
{
    if (const auto command = globalWindowAction(Qt::LeftButton, input()->modifiersRelevantForGlobalShortcuts())) {
        return command;
    } else {
        return window->getMousePressCommand(Qt::LeftButton);
    }
}

static std::optional<Options::MouseCommand> windowActionForTabletButtonPress(TabletToolButtonEvent *event, Window *window)
{
    const auto button = event->button == BTN_STYLUS ? Qt::MiddleButton : Qt::RightButton;
    if (const auto command = globalWindowAction(button, input()->modifiersRelevantForGlobalShortcuts())) {
        return command;
    } else {
        return window->getMousePressCommand(button);
    }
}

std::optional<Options::MouseCommand> globalWindowWheelAction(PointerAxisEvent *event)
{
    if (event->orientation != Qt::Vertical) {
        return std::nullopt;
    }
    if (event->modifiersRelevantForGlobalShortcuts != options->commandAllModifier()) {
        return std::nullopt;
    }
    if (input()->pointer()->isConstrained() || workspace()->globalShortcutsDisabled()) {
        return std::nullopt;
    }
    const auto ret = options->operationWindowMouseWheel(-event->delta);
    if (ret == Options::MouseCommand::MouseNothing) {
        return std::nullopt;
    } else {
        return ret;
    }
}

std::optional<Options::MouseCommand> windowWheelCommand(PointerAxisEvent *event, Window *window)
{
    if (const auto globalCommand = globalWindowWheelAction(event)) {
        return globalCommand;
    } else if (const auto command = window->getWheelCommand(event->orientation)) {
        return command;
    } else {
        return std::nullopt;
    }
}
}

class InternalWindowEventFilter : public InputEventFilter
{
public:
    InternalWindowEventFilter()
        : InputEventFilter(InputFilterOrder::InternalWindow)
    {
        m_touchDevice = std::make_unique<QPointingDevice>(QLatin1String("some touchscreen"), 0, QInputDevice::DeviceType::TouchScreen,
                                                          QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position,
                                                          10, 0, kwinApp()->session()->seat(), QPointingDeviceUniqueId());
        QWindowSystemInterface::registerInputDevice(m_touchDevice.get());

        m_tabletDevice = std::make_unique<QPointingDevice>(QLatin1String("some tablet"), 0, QInputDevice::DeviceType::Stylus,
                                                           QPointingDevice::PointerType::Pen, QInputDevice::Capability::Position | QInputDevice::Capability::ZPosition | QInputDevice::Capability::Pressure,
                                                           10, 0, kwinApp()->session()->seat(), QPointingDeviceUniqueId());
        QWindowSystemInterface::registerInputDevice(m_tabletDevice.get());
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->pointer()->focus())->handle();
        if (!internal) {
            // the handle can be nullptr if the tooltip gets closed while focus updates are blocked
            return false;
        }
        QWindowSystemInterface::handleMouseEvent(internal,
                                                 event->position - internal->position(),
                                                 event->position,
                                                 event->buttons,
                                                 Qt::NoButton,
                                                 QEvent::MouseMove,
                                                 event->modifiers);
        return true;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->pointer()->focus())->handle();
        if (!internal) {
            // the handle can be nullptr if the tooltip gets closed while focus updates are blocked
            return false;
        }
        QWindowSystemInterface::handleMouseEvent(internal,
                                                 event->position - internal->position(),
                                                 event->position,
                                                 event->buttons,
                                                 event->button,
                                                 event->state == PointerButtonState::Pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                                                 event->modifiers);
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (!input()->pointer()->focus() || !input()->pointer()->focus()->isInternal()) {
            return false;
        }
        QWindow *internal = static_cast<InternalWindow *>(input()->pointer()->focus())->handle();
        if (!internal) {
            // the handle can be nullptr if the tooltip gets closed while focus updates are blocked
            return false;
        }
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp);
        QWindowSystemInterface::handleWheelEvent(internal,
                                                 timestamp.count(),
                                                 event->position - internal->position(),
                                                 event->position,
                                                 QPoint(),
                                                 ((event->orientation == Qt::Horizontal) ? QPoint(event->delta, 0) : QPoint(0, event->delta)) * -1,
                                                 event->modifiers,
                                                 Qt::NoScrollPhase,
                                                 Qt::MouseEventNotSynthesized,
                                                 event->inverted);
        return true;
    }

    bool touchDown(TouchDownEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            // something else is getting the events
            return false;
        }
        if (!input()->touch()->focus() || !input()->touch()->focus()->isInternal()) {
            return false;
        }

        const qreal contactAreaWidth = 8;
        const qreal contactAreaHeight = 8;

        auto &touchPoint = m_touchPoints.emplaceBack(QWindowSystemInterface::TouchPoint{});
        touchPoint.id = event->id;
        touchPoint.area = QRectF(event->pos.x() - contactAreaWidth / 2, event->pos.y() - contactAreaHeight / 2, contactAreaWidth, contactAreaHeight);
        touchPoint.state = QEventPoint::State::Pressed;
        touchPoint.pressure = 1;

        QWindow *internal = static_cast<InternalWindow *>(input()->touch()->focus())->handle();
        QWindowSystemInterface::handleTouchEvent(internal, m_touchDevice.get(), m_touchPoints, input()->keyboardModifiers());

        touchPoint.state = QEventPoint::State::Stationary;
        return true;
    }

    bool touchMotion(TouchMotionEvent *event) override
    {
        auto it = std::ranges::find_if(m_touchPoints, [event](const auto &touchPoint) {
            return touchPoint.id == event->id;
        });
        if (it == m_touchPoints.end()) {
            return false;
        }

        it->area.moveCenter(event->pos);
        it->state = QEventPoint::State::Updated;

        if (auto internalWindow = qobject_cast<InternalWindow *>(input()->touch()->focus())) {
            QWindowSystemInterface::handleTouchEvent(internalWindow->handle(), m_touchDevice.get(), m_touchPoints, input()->keyboardModifiers());
        }

        it->state = QEventPoint::State::Stationary;
        return true;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        auto it = std::ranges::find_if(m_touchPoints, [event](const auto &touchPoint) {
            return touchPoint.id == event->id;
        });
        if (it == m_touchPoints.end()) {
            return false;
        }

        it->pressure = 0;
        it->state = QEventPoint::State::Released;

        if (auto internalWindow = qobject_cast<InternalWindow *>(input()->touch()->focus())) {
            QWindowSystemInterface::handleTouchEvent(internalWindow->handle(), m_touchDevice.get(), m_touchPoints, input()->keyboardModifiers());
        }

        m_touchPoints.erase(it);
        return true;
    }
    bool touchCancel() override
    {
        if (!m_touchPoints.isEmpty()) {
            m_touchPoints.clear();
            QWindowSystemInterface::handleTouchCancelEvent(nullptr, m_touchDevice.get());
        }
        return false;
    }

    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        if (!input()->tablet()->focus() || !input()->tablet()->focus()->isInternal()) {
            return false;
        }

        // handleTabletEnterLeaveProximityEvent has lots of parameters, most of which are ignored, so don't bother with them
        QWindowSystemInterface::handleTabletEnterLeaveProximityEvent(nullptr,
                                                                     m_tabletDevice.get(), event->type == TabletToolProximityEvent::EnterProximity);

        return true;
    }

    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        if (!input()->tablet()->focus() || !input()->tablet()->focus()->isInternal()) {
            return false;
        }

        QWindow *internal = static_cast<InternalWindow *>(input()->tablet()->focus())->handle();
        if (!internal) {
            return true;
        }

        const QPointF globalPos = event->position;
        const QPointF localPos = globalPos - internal->position();

        QWindowSystemInterface::handleTabletEvent(internal, std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count(), m_tabletDevice.get(), localPos, globalPos, event->buttons, event->pressure, event->xTilt, event->yTilt, event->sliderPosition, event->rotation, event->distance, input()->keyboardModifiers());

        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        if (!input()->tablet()->focus() || !input()->tablet()->focus()->isInternal()) {
            return false;
        }

        QWindow *internal = static_cast<InternalWindow *>(input()->tablet()->focus())->handle();
        if (!internal) {
            return true;
        }

        const QPointF globalPos = event->position;
        const QPointF localPos = globalPos - internal->position();
        const Qt::MouseButtons buttons = event->type == TabletToolTipEvent::Press ? Qt::LeftButton : Qt::NoButton;

        QWindowSystemInterface::handleTabletEvent(internal, std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count(), m_tabletDevice.get(), localPos, globalPos, buttons, event->pressure, event->xTilt, event->yTilt, event->sliderPosition, event->rotation, event->distance, input()->keyboardModifiers());

        return true;
    }

private:
    std::unique_ptr<QPointingDevice> m_touchDevice;
    std::unique_ptr<QPointingDevice> m_tabletDevice;
    QList<QWindowSystemInterface::TouchPoint> m_touchPoints;
};

class DecorationEventFilter : public InputEventFilter
{
public:
    DecorationEventFilter()
        : InputEventFilter(InputFilterOrder::Decoration)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->position - decoration->window()->pos();
        QHoverEvent e(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationMove(p, event->position);
        return true;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }

        Window *window = decoration->window();
        const QPointF globalPos = event->position;
        const QPointF p = event->position - window->pos();

        if (event->state == PointerButtonState::Pressed) {
            if (const auto command = windowActionForPointerButtonPress(event, window)) {
                if (window->performMousePressCommand(*command, event->position)) {
                    return true;
                }
            }
        }

        QMouseEvent e(event->state == PointerButtonState::Pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, p, event->position, event->button, event->buttons, event->modifiers);
        e.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (e.isAccepted()) {
            // if a non-active window is closed through the decoration, it should be allowed to activate itself
            // TODO use the event serial instead, once that's plumbed through
            const uint32_t serial = waylandServer()->display()->nextSerial();
            const QString token = waylandServer()->xdgActivationIntegration()->requestPrivilegedToken(nullptr, serial, waylandServer()->seat(), window->desktopFileName());
            workspace()->setActivationToken(token, serial, window->desktopFileName());
        }
        if (!e.isAccepted() && event->state == PointerButtonState::Pressed) {
            window->processDecorationButtonPress(p, globalPos, event->button);
        }
        if (event->state == PointerButtonState::Released) {
            window->processDecorationButtonRelease(event->button);
        }
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        if (const auto command = globalWindowWheelAction(event)) {
            if (m_accumulator.accumulate(event)) {
                if (decoration->window()->performMousePressCommand(*command, event->position)) {
                    return true;
                }
            } else if (decoration->window()->mousePressCommandConsumesEvent(*command)) {
                return true;
            }
        }
        const QPointF localPos = event->position - decoration->window()->pos();
        QWheelEvent e(localPos, event->position, QPoint(),
                      (event->orientation == Qt::Horizontal) ? QPoint(event->delta, 0) : QPoint(0, event->delta),
                      event->buttons,
                      event->modifiers,
                      Qt::NoScrollPhase,
                      false);
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration, &e);
        if (e.isAccepted()) {
            return true;
        }
        if ((event->orientation == Qt::Vertical) && decoration->window()->titlebarPositionUnderMouse()) {
            if (float delta = m_accumulator.accumulate(event)) {
                decoration->window()->performMousePressCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                               event->position);
            }
        }
        return true;
    }
    bool touchDown(TouchDownEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        if (input()->touch()->decorationPressId() != -1) {
            // already on a decoration, ignore further touch points, but filter out
            return true;
        }
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }

        input()->touch()->setDecorationPressId(event->id);
        m_lastGlobalTouchPos = event->pos;
        m_lastLocalTouchPos = event->pos - decoration->window()->pos();

        QHoverEvent hoverEvent(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::sendEvent(decoration->decoration(), &hoverEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, event->pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (!e.isAccepted()) {
            decoration->window()->processDecorationButtonPress(m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton);
        }
        return true;
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }
        if (input()->touch()->decorationPressId() == -1) {
            return false;
        }
        if (input()->touch()->decorationPressId() != qint32(event->id)) {
            // ignore, but filter out
            return true;
        }
        m_lastGlobalTouchPos = event->pos;
        m_lastLocalTouchPos = event->pos - decoration->window()->pos();

        QHoverEvent e(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationMove(m_lastLocalTouchPos, event->pos);
        return true;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            // can happen when quick tiling
            if (input()->touch()->decorationPressId() == event->id) {
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
        if (input()->touch()->decorationPressId() != qint32(event->id)) {
            // ignore, but filter out
            return true;
        }

        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationButtonRelease(Qt::LeftButton);

        QHoverEvent leaveEvent(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::sendEvent(decoration->decoration(), &leaveEvent);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setDecorationPressId(-1);
        return true;
    }

    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        auto decoration = input()->tablet()->decoration();
        if (!decoration) {
            return false;
        }
        if (event->type == TabletToolProximityEvent::EnterProximity) {
            const QPointF p = event->position - decoration->window()->pos();
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->window()->processDecorationMove(p, event->position);
        } else {
            QHoverEvent leaveEvent(QEvent::HoverLeave, QPointF(), QPointF());
            QCoreApplication::sendEvent(decoration->decoration(), &leaveEvent);
        }

        return true;
    }

    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        auto decoration = input()->tablet()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->position - decoration->window()->pos();

        QHoverEvent e(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->window()->processDecorationMove(p, event->position);

        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        auto decoration = input()->tablet()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF globalPos = event->position;
        const QPointF p = event->position - decoration->window()->pos();

        const bool isPressed = event->type == TabletToolTipEvent::Press;
        QMouseEvent e(isPressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                      p,
                      event->position,
                      Qt::LeftButton,
                      isPressed ? Qt::LeftButton : Qt::MouseButtons(),
                      input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (!e.isAccepted() && isPressed) {
            decoration->window()->processDecorationButtonPress(p, globalPos, Qt::LeftButton);
        }
        if (event->type == TabletToolTipEvent::Release) {
            decoration->window()->processDecorationButtonRelease(Qt::LeftButton);
        }

        return true;
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
    TabBoxInputFilter()
        : InputEventFilter(InputFilterOrder::TabBox)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        QMouseEvent mouseEvent(QEvent::MouseMove,
                               event->position,
                               event->position,
                               Qt::NoButton,
                               event->buttons,
                               event->modifiers);
        mouseEvent.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        mouseEvent.setAccepted(false);
        return workspace()->tabbox()->handleMouseEvent(&mouseEvent);
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        QMouseEvent mouseEvent(event->state == PointerButtonState::Pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                               event->position,
                               event->position,
                               event->button,
                               event->buttons,
                               event->modifiers);
        mouseEvent.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        mouseEvent.setAccepted(false);
        return workspace()->tabbox()->handleMouseEvent(&mouseEvent);
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }

        if (event->state == KeyboardKeyState::Repeated || event->state == KeyboardKeyState::Pressed) {
            workspace()->tabbox()->keyPress(*event);
        } else if (event->modifiersRelevantForGlobalShortcuts == Qt::NoModifier) {
            workspace()->tabbox()->modifiersReleased();
            return false;
        }
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (!workspace()->tabbox() || !workspace()->tabbox()->isGrabbed()) {
            return false;
        }
        QWheelEvent wheelEvent(event->position,
                               event->position,
                               QPoint(),
                               (event->orientation == Qt::Horizontal) ? QPoint(event->delta, 0) : QPoint(0, event->delta),
                               event->buttons,
                               event->modifiers,
                               Qt::NoScrollPhase,
                               event->inverted);
        wheelEvent.setAccepted(false);
        return workspace()->tabbox()->handleWheelEvent(&wheelEvent);
    }
};
#endif

class ScreenEdgeInputFilter : public InputEventFilter
{
public:
    ScreenEdgeInputFilter()
        : InputEventFilter(InputFilterOrder::ScreenEdge)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        workspace()->screenEdges()->handlePointerMotion(event->position, event->timestamp);
        // always forward
        return false;
    }
    bool touchDown(TouchDownEvent *event) override
    {
        return workspace()->screenEdges()->gestureRecognizer()->touchDown(event->id, event->pos);
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        return workspace()->screenEdges()->gestureRecognizer()->touchMotion(event->id, event->pos);
    }
    bool touchUp(TouchUpEvent *event) override
    {
        return workspace()->screenEdges()->gestureRecognizer()->touchUp(event->id);
    }
    bool touchCancel() override
    {
        workspace()->screenEdges()->gestureRecognizer()->touchCancel();
        return false;
    }
};

/**
 * This filter implements window actions. If the event should not be passed to the
 * current window it will filter out the event
 */
class WindowActionInputFilter : public InputEventFilter
{
public:
    WindowActionInputFilter()
        : InputEventFilter(InputFilterOrder::WindowAction)
    {
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        if (event->state == PointerButtonState::Pressed) {
            Window *window = input()->pointer()->focus();
            if (!window || !window->isClient()) {
                return false;
            }
            if (const auto command = windowActionForPointerButtonPress(event, window)) {
                return window->performMousePressCommand(*command, event->position);
            }
            return false;
        } else {
            // because of implicit pointer grab while a button is pressed, this may need to
            // target a different window than the one with pointer focus
            Window *window = input()->pointer()->hover();
            if (!window || !window->isClient()) {
                return false;
            }
            if (const auto command = window->getMouseReleaseCommand(event->button)) {
                window->performMouseReleaseCommand(*command, event->position);
            }
        }
        return false;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        if (event->orientation != Qt::Vertical) {
            // only actions on vertical scroll
            return false;
        }
        Window *window = input()->pointer()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        const auto command = windowWheelCommand(event, window);
        if (!command) {
            return false;
        }
        if (!m_accumulator.accumulate(event)) {
            return window->mousePressCommandConsumesEvent(*command);
        }
        return window->performMousePressCommand(*command, event->position);
    }
    bool touchDown(TouchDownEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        Window *window = input()->touch()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        if (const auto command = windowActionForTouchDown(window)) {
            return window->performMousePressCommand(*command, event->pos);
        }
        return false;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        Window *window = input()->touch()->focus();
        if (!window || !window->isClient()) {
            return false;
        }
        const auto command = window->getMouseReleaseCommand(Qt::LeftButton);
        if (command) {
            window->performMouseReleaseCommand(*command, input()->touch()->position());
        }
        return false;
    }
    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        Window *window = input()->tablet()->focus();
        if (!window || !window->isClient()) {
            return false;
        }

        if (event->type == TabletToolTipEvent::Press) {
            if (const auto command = windowActionForTabletTipDown(window)) {
                return window->performMousePressCommand(*command, event->position);
            }
        } else {
            const auto command = window->getMouseReleaseCommand(Qt::LeftButton);
            if (command) {
                window->performMouseReleaseCommand(*command, event->position);
            }
        }

        return false;
    }
    bool tabletToolButtonEvent(TabletToolButtonEvent *event) override
    {
        Window *window = input()->tablet()->focus();
        if (!window || !window->isClient()) {
            return false;
        }

        if (event->pressed) {
            if (const auto command = windowActionForTabletButtonPress(event, window)) {
                return window->performMousePressCommand(*command, input()->tablet()->position());
            }
        } else {
            const auto command = window->getMouseReleaseCommand(event->button == BTN_STYLUS ? Qt::MiddleButton : Qt::RightButton);
            if (command) {
                window->performMouseReleaseCommand(*command, input()->tablet()->position());
            }
        }

        return false;
    }

private:
    MouseWheelAccumulator m_accumulator;
};

class InputMethodEventFilter : public InputEventFilter
{
public:
    InputMethodEventFilter()
        : InputEventFilter(InputFilterOrder::InputMethod)
    {
    }

    bool pointerButton(PointerButtonEvent *event) override
    {
        auto inputMethod = kwinApp()->inputMethod();
        if (!inputMethod) {
            return false;
        }
        if (event->state != PointerButtonState::Pressed) {
            return false;
        }

        // clicking on an on screen keyboard shouldn't flush, check we're clicking on our target window
        if (input()->pointer()->focus() != inputMethod->activeWindow()) {
            return false;
        }

        inputMethod->commitPendingText();
        return false;
    }

    bool touchDown(TouchDownEvent *event) override
    {
        auto inputMethod = kwinApp()->inputMethod();
        if (!inputMethod) {
            return false;
        }
        if (input()->findToplevel(event->pos) != inputMethod->activeWindow()) {
            return false;
        }

        inputMethod->commitPendingText();
        return false;
    }

    bool keyboardKey(KeyboardKeyEvent *event) override
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
    ForwardInputFilter()
        : InputEventFilter(InputFilterOrder::Forward)
    {
    }
    bool pointerMotion(PointerMotionEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp);
        seat->notifyPointerMotion(event->position);
        // absolute motion events confuse games and Wayland doesn't have a warp event yet
        // -> send a relative motion event with a zero delta to signal the warp instead
        if (event->warp) {
            seat->relativePointerMotion(QPointF(0, 0), QPointF(0, 0), event->timestamp);
        } else if (!event->delta.isNull()) {
            seat->relativePointerMotion(event->delta, event->deltaUnaccelerated, event->timestamp);
        }
        return true;
    }
    bool pointerButton(PointerButtonEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp);
        seat->notifyPointerButton(event->nativeButton, event->state);
        return true;
    }
    bool pointerFrame() override
    {
        auto seat = waylandServer()->seat();
        seat->notifyPointerFrame();
        return true;
    }
    bool pointerAxis(PointerAxisEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp);
        seat->notifyPointerAxis(event->orientation, event->delta, event->deltaV120, event->source, event->inverted);
        return true;
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        input()->keyboard()->update();
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp);
        seat->notifyKeyboardKey(event->nativeScanCode, event->state, event->serial);
        return true;
    }
    bool touchDown(TouchDownEvent *event) override
    {
        auto seat = waylandServer()->seat();
        auto w = input()->findToplevel(event->pos);
        if (!w) {
            qCCritical(KWIN_CORE) << "Could not touch down, there's no window under" << event->pos;
            return false;
        }
        seat->setTimestamp(event->time);
        auto tp = seat->notifyTouchDown(w->surface(), w->bufferGeometry().topLeft(), event->id, event->pos);
        if (!tp) {
            qCCritical(KWIN_CORE) << "Could not touch down" << event->pos;
            return false;
        }
        QObject::connect(w, &Window::bufferGeometryChanged, tp, [w, tp]() {
            tp->setSurfacePosition(w->bufferGeometry().topLeft());
        });
        return true;
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->notifyTouchMotion(event->id, event->pos);
        return true;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->notifyTouchUp(event->id);
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
    bool pinchGestureBegin(PointerPinchGestureBeginEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->startPointerPinchGesture(event->fingerCount);
        return true;
    }
    bool pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->updatePointerPinchGesture(event->delta, event->scale, event->angleDelta);
        return true;
    }
    bool pinchGestureEnd(PointerPinchGestureEndEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->endPointerPinchGesture();
        return true;
    }
    bool pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->cancelPointerPinchGesture();
        return true;
    }

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->startPointerSwipeGesture(event->fingerCount);
        return true;
    }
    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->updatePointerSwipeGesture(event->delta);
        return true;
    }
    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->endPointerSwipeGesture();
        return true;
    }
    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->cancelPointerSwipeGesture();
        return true;
    }
    bool holdGestureBegin(PointerHoldGestureBeginEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->startPointerHoldGesture(event->fingerCount);
        return true;
    }
    bool holdGestureEnd(PointerHoldGestureEndEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->endPointerHoldGesture();
        return true;
    }
    bool holdGestureCancelled(PointerHoldGestureCancelEvent *event) override
    {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->time);
        seat->cancelPointerHoldGesture();
        return true;
    }

    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        Window *window = input()->tablet()->focus();
        if (!window || !window->surface()) {
            return false;
        }

        TabletSeatV2Interface *seat = waylandServer()->tabletManagerV2()->seat(waylandServer()->seat());
        TabletToolV2Interface *tool = seat->tool(event->tool);
        TabletV2Interface *tablet = seat->tablet(event->device);

        const auto [surface, surfaceLocalPos] = window->surface()->mapToInputSurface(window->mapToLocal(event->position));
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || !tablet->isSurfaceSupported(surface)) {
            return emulateTabletEvent(event);
        }

        if (event->type == TabletToolProximityEvent::EnterProximity) {
            tool->sendProximityIn(tablet);
            tool->sendMotion(surfaceLocalPos);
        } else {
            tool->sendProximityOut();
        }

        if (tool->hasCapability(TabletToolV2Interface::Tilt)) {
            tool->sendTilt(event->xTilt, event->yTilt);
        }
        if (tool->hasCapability(TabletToolV2Interface::Rotation)) {
            tool->sendRotation(event->rotation);
        }
        if (tool->hasCapability(TabletToolV2Interface::Distance)) {
            tool->sendDistance(event->distance);
        }
        if (tool->hasCapability(TabletToolV2Interface::Slider)) {
            tool->sendSlider(event->sliderPosition);
        }

        tool->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        return true;
    }

    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        Window *window = input()->tablet()->focus();
        if (!window || !window->surface()) {
            return false;
        }

        TabletSeatV2Interface *seat = waylandServer()->tabletManagerV2()->seat(waylandServer()->seat());
        TabletToolV2Interface *tool = seat->tool(event->tool);
        TabletV2Interface *tablet = seat->tablet(event->device);

        const auto [surface, surfaceLocalPos] = window->surface()->mapToInputSurface(window->mapToLocal(event->position));
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || !tablet->isSurfaceSupported(surface)) {
            return emulateTabletEvent(event);
        }

        tool->sendMotion(surfaceLocalPos);

        if (tool->hasCapability(TabletToolV2Interface::Pressure)) {
            tool->sendPressure(event->pressure);
        }
        if (tool->hasCapability(TabletToolV2Interface::Tilt)) {
            tool->sendTilt(event->xTilt, event->yTilt);
        }
        if (tool->hasCapability(TabletToolV2Interface::Rotation)) {
            tool->sendRotation(event->rotation);
        }
        if (tool->hasCapability(TabletToolV2Interface::Distance)) {
            tool->sendDistance(event->distance);
        }
        if (tool->hasCapability(TabletToolV2Interface::Slider)) {
            tool->sendSlider(event->sliderPosition);
        }

        tool->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        Window *window = input()->tablet()->focus();
        if (!window || !window->surface()) {
            return false;
        }

        TabletSeatV2Interface *seat = waylandServer()->tabletManagerV2()->seat(waylandServer()->seat());
        TabletToolV2Interface *tool = seat->tool(event->tool);
        TabletV2Interface *tablet = seat->tablet(event->device);

        const auto [surface, surfaceLocalPos] = window->surface()->mapToInputSurface(window->mapToLocal(event->position));
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || !tablet->isSurfaceSupported(surface)) {
            return emulateTabletEvent(event);
        }

        if (event->type == TabletToolTipEvent::Press) {
            tool->sendMotion(surfaceLocalPos);
            tool->sendDown();
        } else {
            tool->sendUp();
        }

        if (tool->hasCapability(TabletToolV2Interface::Pressure)) {
            tool->sendPressure(event->pressure);
        }
        if (tool->hasCapability(TabletToolV2Interface::Tilt)) {
            tool->sendTilt(event->xTilt, event->yTilt);
        }
        if (tool->hasCapability(TabletToolV2Interface::Rotation)) {
            tool->sendRotation(event->rotation);
        }
        if (tool->hasCapability(TabletToolV2Interface::Distance)) {
            tool->sendDistance(event->distance);
        }
        if (tool->hasCapability(TabletToolV2Interface::Slider)) {
            tool->sendSlider(event->sliderPosition);
        }

        tool->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp).count());
        return true;
    }

    bool emulateTabletEvent(TabletToolProximityEvent *event)
    {
        // Tablet input emulation is deprecated. It will be removed in the near future.
        static bool emulateInput = qEnvironmentVariableIntValue("KWIN_WAYLAND_EMULATE_TABLET") == 1;
        if (!emulateInput) {
            return false;
        }

        switch (event->type) {
        case TabletToolProximityEvent::EnterProximity:
            input()->pointer()->processMotionAbsolute(event->position, event->timestamp);
            break;
        case TabletToolProximityEvent::LeaveProximity:
            break;
        }
        return true;
    }

    bool emulateTabletEvent(TabletToolTipEvent *event)
    {
        // Tablet input emulation is deprecated. It will be removed in the near future.
        static bool emulateInput = qEnvironmentVariableIntValue("KWIN_WAYLAND_EMULATE_TABLET") == 1;
        if (!emulateInput) {
            return false;
        }

        switch (event->type) {
        case TabletToolTipEvent::Press:
            input()->pointer()->processButton(qtMouseButtonToButton(Qt::LeftButton),
                                              PointerButtonState::Pressed, event->timestamp);
            break;
        case TabletToolTipEvent::Release:
            input()->pointer()->processButton(qtMouseButtonToButton(Qt::LeftButton),
                                              PointerButtonState::Released, event->timestamp);
            break;
        }
        return true;
    }

    bool emulateTabletEvent(TabletToolAxisEvent *event)
    {
        // Tablet input emulation is deprecated. It will be removed in the near future.
        static bool emulateInput = qEnvironmentVariableIntValue("KWIN_WAYLAND_EMULATE_TABLET") == 1;
        if (!emulateInput) {
            return false;
        }

        input()->pointer()->processMotionAbsolute(event->position, event->timestamp);

        return true;
    }

    bool tabletToolButtonEvent(TabletToolButtonEvent *event) override
    {
        TabletToolV2Interface *tool = waylandServer()->tabletManagerV2()->seat(waylandServer()->seat())->tool(event->tool);
        if (!tool->isClientSupported()) {
            return false;
        }
        tool->sendButton(event->button, event->pressed);
        return true;
    }

    TabletPadV2Interface *findAndAdoptPad(InputDevice *device) const
    {
        Window *window = workspace()->activeWindow();
        TabletSeatV2Interface *seat = waylandServer()->tabletManagerV2()->seat(waylandServer()->seat());
        if (!window || !window->surface() || !seat->isClientSupported(window->surface()->client())) {
            return nullptr;
        }

        TabletPadV2Interface *pad = seat->pad(device);
        if (!pad) {
            return nullptr;
        }

        TabletV2Interface *tablet = seat->matchingTablet(pad);
        if (!tablet) {
            return nullptr;
        }

        pad->setCurrentSurface(window->surface(), tablet);
        return pad;
    }

    bool tabletPadButtonEvent(TabletPadButtonEvent *event) override
    {
        auto pad = findAndAdoptPad(event->device);
        if (!pad) {
            return false;
        }

        auto group = pad->group(event->group);
        if (event->isModeSwitch) {
            group->setCurrentMode(event->mode);
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(event->time).count();
            group->sendModeSwitch(milliseconds);
            // TODO send button to app?
        }

        pad->sendButton(event->time, event->button, event->pressed);
        return true;
    }

    bool tabletPadRingEvent(TabletPadRingEvent *event) override
    {
        auto pad = findAndAdoptPad(event->device);
        if (!pad) {
            return false;
        }
        auto ring = pad->group(event->group)->ring(event->number);

        if (event->isFinger && event->position == -1) {
            ring->sendStop();
        } else {
            ring->sendAngle(event->position);
        }

        if (event->isFinger) {
            ring->sendSource(TabletPadRingV2Interface::SourceFinger);
        }
        ring->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->time).count());
        return true;
    }

    bool tabletPadStripEvent(TabletPadStripEvent *event) override
    {
        auto pad = findAndAdoptPad(event->device);
        if (!pad) {
            return false;
        }
        auto strip = pad->group(event->group)->strip(event->number);

        strip->sendPosition(event->position);
        if (event->isFinger) {
            strip->sendSource(TabletPadStripV2Interface::SourceFinger);
        }
        strip->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->time).count());
        return true;
    }

    bool tabletPadDialEvent(TabletPadDialEvent *event) override
    {
        auto pad = findAndAdoptPad(event->device);
        if (!pad) {
            return false;
        }
        auto dial = pad->group(event->group)->dial(event->number);

        dial->sendDelta(event->delta);

        dial->sendFrame(std::chrono::duration_cast<std::chrono::milliseconds>(event->time).count());
        return true;
    }
};

static AbstractDropHandler *dropHandler(Window *window)
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
#if KWIN_BUILD_X11
    if (qobject_cast<X11Window *>(window) && kwinApp()->xwayland()) {
        return kwinApp()->xwayland()->xwlDropHandler();
    }
#endif

    return nullptr;
}

class DragAndDropInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    DragAndDropInputFilter()
        : InputEventFilter(InputFilterOrder::DragAndDrop)
    {
        connect(waylandServer()->seat(), &SeatInterface::dragRequested, this, [](AbstractDataSource *source, SurfaceInterface *origin, quint32 serial, DragAndDropIcon *dragIcon) {
            if (auto window = waylandServer()->findWindow(origin->mainSurface())) {
                QMatrix4x4 transformation = window->inputTransformation();
                transformation.translate(-QVector3D(origin->mapToMainSurface(QPointF(0, 0))));

                if (waylandServer()->seat()->hasImplicitPointerGrab(serial)) {
                    if (waylandServer()->seat()->startPointerDrag(source, origin, waylandServer()->seat()->pointerPos(), transformation, serial, dragIcon)) {
                        return;
                    }
                }

                if (const auto touchPoint = waylandServer()->seat()->touchPointByImplicitGrabSerial(serial)) {
                    if (waylandServer()->seat()->startTouchDrag(source, origin, touchPoint->position, transformation, serial, dragIcon)) {
                        return;
                    }
                }

                if (waylandServer()->tabletManagerV2()->seat(waylandServer()->seat())->hasImplicitGrab(serial)) {
                    if (waylandServer()->seat()->startTabletDrag(source, origin, input()->tablet()->position(), transformation, serial, dragIcon)) {
                        return;
                    }
                }
            }

            if (source) {
                source->dndCancelled();
            }
        });

        connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, [this]() {
            AbstractDataSource *dragSource = waylandServer()->seat()->dragSource();
            if (!dragSource) {
                return;
            }

            dragSource->setKeyboardModifiers(input()->keyboardModifiers());
            connect(input(), &InputRedirection::keyboardModifiersChanged, dragSource, [dragSource](Qt::KeyboardModifiers mods) {
                dragSource->setKeyboardModifiers(mods);
            });
            auto toplevelDrag = waylandServer()->seat()->xdgTopleveldrag();
            if (!toplevelDrag) {
                return;
            }
            setupDraggedToplevel();
            connect(toplevelDrag, &XdgToplevelDragV1Interface::toplevelChanged, this, &DragAndDropInputFilter::setupDraggedToplevel);
        });

        connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, [this] {
            m_dragTarget = nullptr;
            m_lastPos.reset();
            if (m_currentToplevelDragWindow) {
                m_currentToplevelDragWindow->setKeepAbove(m_wasKeepAbove);
                m_currentToplevelDragWindow->endInteractiveMoveResize();
                workspace()->raiseWindow(m_currentToplevelDragWindow);
                workspace()->requestFocus(m_currentToplevelDragWindow);
                m_currentToplevelDragWindow = nullptr;
            }
        });

        m_raiseTimer.setSingleShot(true);
        m_raiseTimer.setInterval(1000);
        connect(&m_raiseTimer, &QTimer::timeout, this, &DragAndDropInputFilter::raiseDragTarget);
    }

    bool pointerMotion(PointerMotionEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragPointer()) {
            return false;
        }
        if (seat->isDragTouch()) {
            return true;
        }

        motion(event->position, event->timestamp);
        return true;
    }

    bool pointerButton(PointerButtonEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragPointer()) {
            return false;
        }
        if (seat->isDragTouch()) {
            return true;
        }
        seat->setTimestamp(event->timestamp);
        if (event->state == PointerButtonState::Pressed) {
            seat->notifyPointerButton(event->nativeButton, event->state);
        } else {
            raiseDragTarget();
            m_dragTarget = nullptr;
            seat->notifyPointerButton(event->nativeButton, event->state);
        }
        // TODO: should we pass through effects?
        return true;
    }

    bool pointerFrame() override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragPointer()) {
            return false;
        }
        if (seat->isDragTouch()) {
            return true;
        }

        seat->notifyPointerFrame();
        return true;
    }

    bool touchDown(TouchDownEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isDragPointer()) {
            return true;
        }
        return seat->isDragTouch();
    }
    bool touchMotion(TouchMotionEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (seat->isDragPointer()) {
            return true;
        }
        if (!seat->isDragTouch()) {
            return false;
        }

        const auto touchPoint = seat->touchPointByImplicitGrabSerial(*seat->dragSerial());
        if (!touchPoint || touchPoint->id != event->id) {
            return true;
        }

        motion(event->pos, event->time);
        return true;
    }
    bool touchUp(TouchUpEvent *event) override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragTouch()) {
            return false;
        }
        seat->setTimestamp(event->time);
        const auto touchPoint = seat->touchPointByImplicitGrabSerial(*seat->dragSerial());
        if (touchPoint && touchPoint->id == event->id) {
            seat->endDrag();
            raiseDragTarget();
        }
        seat->notifyTouchUp(event->id);
        return true;
    }
    bool touchCancel() override
    {
        auto seat = waylandServer()->seat();
        if (!seat->isDragTouch()) {
            return false;
        }
        seat->cancelDrag();
        seat->notifyTouchCancel();
        return true;
    }
    bool keyboardKey(KeyboardKeyEvent *event) override
    {
        if (event->key != Qt::Key_Escape) {
            return false;
        }

        auto seat = waylandServer()->seat();
        if (!seat->isDrag()) {
            return false;
        }

        seat->cancelDrag();

        return true;
    }

    bool tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        SeatInterface *seat = waylandServer()->seat();
        if (!seat->isDragTablet()) {
            return false;
        }

        return true;
    }

    bool tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        SeatInterface *seat = waylandServer()->seat();
        if (!seat->isDragTablet()) {
            return false;
        }

        TabletToolV2Interface *dragTool = waylandServer()->tabletManagerV2()->seat(seat)->toolByImplicitGrabSerial(*seat->dragSerial());
        if (!dragTool || dragTool->device() != event->tool) {
            return true;
        }

        motion(event->position, event->timestamp);
        return true;
    }

    bool tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        SeatInterface *seat = waylandServer()->seat();
        if (!seat->isDragTablet()) {
            return false;
        }

        if (event->type == TabletToolTipEvent::Release) {
            seat->endDrag();
        }

        return true;
    }

    bool tabletToolButtonEvent(TabletToolButtonEvent *event) override
    {
        SeatInterface *seat = waylandServer()->seat();
        if (!seat->isDragTablet()) {
            return false;
        }

        return true;
    }

private:
    void raiseDragTarget()
    {
        m_raiseTimer.stop();
        if (m_dragTarget) {
            workspace()->raiseWindow(m_dragTarget);
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
            if (auto toplevelDrag = waylandServer()->seat()->xdgTopleveldrag(); toplevelDrag && toplevelDrag->toplevel() && toplevelDrag->toplevel()->surface() == window->surface()) {
                continue;
            }
            if (window->isDeleted()) {
                continue;
            }
            if (!window->isClient()) {
                continue;
            }
            if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop() || window->isMinimized() || window->isHidden() || window->isHiddenByShowDesktop()) {
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

    void setupDraggedToplevel()
    {
        auto toplevelDrag = waylandServer()->seat()->xdgTopleveldrag();
        if (!toplevelDrag) {
            return;
        }
        auto window = toplevelDrag->toplevel() ? waylandServer()->findWindow(toplevelDrag->toplevel()->surface()) : nullptr;
        if (window == m_currentToplevelDragWindow || !window) {
            return;
        }
        if (!window->readyForPainting()) {
            connect(window, &Window::readyForPaintingChanged, this, &DragAndDropInputFilter::setupDraggedToplevel, Qt::SingleShotConnection);
            return;
        }
        const auto currentPosition = waylandServer()->seat()->isDragPointer() ? input()->pointer()->pos() : input()->tablet()->position();
        m_wasKeepAbove = window->keepAbove();
        window->move(currentPosition - waylandServer()->seat()->xdgTopleveldrag()->offset());
        window->setKeepAbove(true);
        window->performMousePressCommand(Options::MouseMove, currentPosition);
        m_currentToplevelDragWindow = window;
    }

    void motion(const QPointF &position, std::chrono::microseconds time)
    {
        SeatInterface *seat = waylandServer()->seat();
        seat->setTimestamp(time);

        if (auto window = m_currentToplevelDragWindow) {
            window->updateInteractiveMoveResize(position, input()->keyboardModifiers());
        }

        Window *dragTarget = pickDragTarget(position);
        if (dragTarget) {
            if (dragTarget != m_dragTarget) {
                workspace()->requestFocus(dragTarget);
                m_raiseTimer.start();
            }
            if (!m_lastPos || (position - *m_lastPos).manhattanLength() > 10) {
                m_lastPos = position;
                // reset timer to delay raising the window
                m_raiseTimer.start();
            }
        }
        m_dragTarget = dragTarget;

        if (auto *xwl = kwinApp()->xwayland()) {
            if (xwl->dragMoveFilter(dragTarget, position)) {
                return;
            }
        }

        if (dragTarget && dragTarget->surface()) {
            const auto [effectiveSurface, offset] = dragTarget->surface()->mapToInputSurface(dragTarget->mapToLocal(position));
            if (seat->dragSurface() != effectiveSurface) {
                QMatrix4x4 inputTransformation = dragTarget->inputTransformation();
                inputTransformation.translate(-QVector3D(effectiveSurface->mapToMainSurface(QPointF(0, 0))));
                seat->setDragTarget(dropHandler(dragTarget), effectiveSurface, position, inputTransformation);
            } else {
                seat->notifyDragMotion(position);
            }
        } else {
            // no window at that place, if we have a surface we need to reset
            seat->setDragTarget(nullptr, nullptr, QPointF(), QMatrix4x4());
        }
    }

    std::optional<QPointF> m_lastPos = std::nullopt;
    QPointer<Window> m_dragTarget;
    QTimer m_raiseTimer;
    QPointer<Window> m_currentToplevelDragWindow = nullptr;
    bool m_wasKeepAbove = false;
};

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
    , m_keyboard(new KeyboardInputRedirection(this))
    , m_pointer(new PointerInputRedirection(this))
    , m_tablet(new TabletInputRedirection(this))
    , m_touch(new TouchInputRedirection(this))
#if KWIN_BUILD_GLOBALSHORTCUTS
    , m_shortcuts(new GlobalShortcutsManager(this))
#endif
{
    setupInputBackends();

    connect(kwinApp(), &Application::workspaceCreated, this, &InputRedirection::setupWorkspace);
}

InputRedirection::~InputRedirection()
{
    m_inputBackends.clear();
    m_inputDevices.clear();

    s_self = nullptr;
}

void InputRedirection::installInputEventFilter(InputEventFilter *filter)
{
    Q_ASSERT(!m_filters.contains(filter));

    auto it = std::lower_bound(m_filters.begin(), m_filters.end(), filter, [](InputEventFilter *a, InputEventFilter *b) {
        return a->weight() < b->weight();
    });
    m_filters.insert(it, filter);
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
    m_inputConfigWatcher = KConfigWatcher::create(kwinApp()->inputConfig());
    connect(m_inputConfigWatcher.data(), &KConfigWatcher::configChanged,
            this, &InputRedirection::handleInputConfigChanged);
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->init();
#endif
}

void InputRedirection::setupWorkspace()
{
    connect(workspace(), &Workspace::outputsChanged, this, &InputRedirection::updateScreens);

    m_keyboard->init();
    m_pointer->init();
    m_touch->init();
    m_tablet->init();

    updateLeds(m_keyboard->xkb()->leds());
    connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, this, &InputRedirection::updateLeds);

    setupInputFilters();
    updateScreens();
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

void InputRedirection::setLastInteractionSerial(uint32_t serial)
{
    m_lastInteractionSerial = serial;
    workspace()->setWasUserInteraction();
}

uint32_t InputRedirection::lastInteractionSerial() const
{
    return m_lastInteractionSerial;
}

void InputRedirection::setLastInputHandler(QObject *device)
{
    m_lastInputDevice = device;
}

class UserActivitySpy : public InputEventSpy
{
public:
    void pointerMotion(PointerMotionEvent *event) override
    {
        if (!event->warp) {
            notifyActivity();
        }
    }
    void pointerButton(PointerButtonEvent *event) override
    {
        notifyActivity();
    }
    void pointerAxis(PointerAxisEvent *event) override
    {
        notifyActivity();
    }

    void keyboardKey(KeyboardKeyEvent *event) override
    {
        notifyActivity();
    }

    void touchDown(TouchDownEvent *event) override
    {
        notifyActivity();
    }
    void touchMotion(TouchMotionEvent *event) override
    {
        notifyActivity();
    }
    void touchUp(TouchUpEvent *event) override
    {
        notifyActivity();
    }

    void pinchGestureBegin(PointerPinchGestureBeginEvent *event) override
    {
        notifyActivity();
    }
    void pinchGestureUpdate(PointerPinchGestureUpdateEvent *event) override
    {
        notifyActivity();
    }
    void pinchGestureEnd(PointerPinchGestureEndEvent *event) override
    {
        notifyActivity();
    }
    void pinchGestureCancelled(PointerPinchGestureCancelEvent *event) override
    {
        notifyActivity();
    }

    void swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        notifyActivity();
    }
    void swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        notifyActivity();
    }
    void swipeGestureEnd(PointerSwipeGestureEndEvent *event) override
    {
        notifyActivity();
    }
    void swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override
    {
        notifyActivity();
    }

    void holdGestureBegin(PointerHoldGestureBeginEvent *event) override
    {
        notifyActivity();
    }
    void holdGestureEnd(PointerHoldGestureEndEvent *event) override
    {
        notifyActivity();
    }
    void holdGestureCancelled(PointerHoldGestureCancelEvent *event) override
    {
        notifyActivity();
    }

    void tabletToolProximityEvent(TabletToolProximityEvent *event) override
    {
        notifyActivity();
    }
    void tabletToolAxisEvent(TabletToolAxisEvent *event) override
    {
        notifyActivity();
    }
    void tabletToolTipEvent(TabletToolTipEvent *event) override
    {
        notifyActivity();
    }
    void tabletToolButtonEvent(TabletToolButtonEvent *event) override
    {
        notifyActivity();
    }
    void tabletPadButtonEvent(TabletPadButtonEvent *event) override
    {
        notifyActivity();
    }
    void tabletPadStripEvent(TabletPadStripEvent *event) override
    {
        notifyActivity();
    }
    void tabletPadRingEvent(TabletPadRingEvent *event) override
    {
        notifyActivity();
    }
    void tabletPadDialEvent(TabletPadDialEvent *event) override
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
    if (kwinApp()->session()->capabilities() & Session::Capability::SwitchTerminal) {
        m_virtualTerminalFilter = std::make_unique<VirtualTerminalFilter>();
        installInputEventFilter(m_virtualTerminalFilter.get());
    }

    m_hideCursorSpy = std::make_unique<HideCursorSpy>();
    installInputEventSpy(m_hideCursorSpy.get());

    m_userActivitySpy = std::make_unique<UserActivitySpy>();
    installInputEventSpy(m_userActivitySpy.get());

#if KWIN_BUILD_SCREENLOCKER
    m_lockscreenFilter = std::make_unique<LockScreenFilter>();
    installInputEventFilter(m_lockscreenFilter.get());
#endif

    if (kwinApp()->supportsGlobalShortcuts()) {
        m_screenEdgeFilter = std::make_unique<ScreenEdgeInputFilter>();
        installInputEventFilter(m_screenEdgeFilter.get());
    }

    m_dragAndDropFilter = std::make_unique<DragAndDropInputFilter>();
    installInputEventFilter(m_dragAndDropFilter.get());

    m_windowSelector = std::make_unique<WindowSelectorFilter>();
    installInputEventFilter(m_windowSelector.get());

#if KWIN_BUILD_TABBOX
    m_tabboxFilter = std::make_unique<TabBoxInputFilter>();
    installInputEventFilter(m_tabboxFilter.get());
#endif
#if KWIN_BUILD_GLOBALSHORTCUTS
    if (kwinApp()->supportsGlobalShortcuts()) {
        m_globalShortcutFilter = std::make_unique<GlobalShortcutFilter>();
        installInputEventFilter(m_globalShortcutFilter.get());
    }
#endif

    m_effectsFilter = std::make_unique<EffectsFilter>();
    installInputEventFilter(m_effectsFilter.get());

    m_interactiveMoveResizeFilter = std::make_unique<MoveResizeFilter>();
    installInputEventFilter(m_interactiveMoveResizeFilter.get());

    m_popupFilter = std::make_unique<PopupInputFilter>();
    installInputEventFilter(m_popupFilter.get());

    m_decorationFilter = std::make_unique<DecorationEventFilter>();
    installInputEventFilter(m_decorationFilter.get());

    m_windowActionFilter = std::make_unique<WindowActionInputFilter>();
    installInputEventFilter(m_windowActionFilter.get());

    m_internalWindowFilter = std::make_unique<InternalWindowEventFilter>();
    installInputEventFilter(m_internalWindowFilter.get());

    m_inputKeyboardFilter = std::make_unique<InputMethodEventFilter>();
    installInputEventFilter(m_inputKeyboardFilter.get());

    m_forwardFilter = std::make_unique<ForwardInputFilter>();
    installInputEventFilter(m_forwardFilter.get());
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
    connect(device, &InputDevice::pointerFrame,
            m_pointer, &PointerInputRedirection::processFrame);
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

    connect(device, &InputDevice::switchToggle, this, [this](SwitchState state, std::chrono::microseconds time, InputDevice *device) {
        SwitchEvent event{
            .device = device,
            .state = state,
            .timestamp = time,
        };
        processSpies(&InputEventSpy::switchEvent, &event);
        processFilters(&InputEventFilter::switchEvent, &event);
    });

    connect(device, &InputDevice::tabletToolAxisEvent,
            m_tablet, &TabletInputRedirection::tabletToolAxisEvent);
    connect(device, &InputDevice::tabletToolProximityEvent,
            m_tablet, &TabletInputRedirection::tabletToolProximityEvent);
    connect(device, &InputDevice::tabletToolTipEvent,
            m_tablet, &TabletInputRedirection::tabletToolTipEvent);
    connect(device, &InputDevice::tabletToolAxisEventRelative,
            m_tablet, &TabletInputRedirection::tabletToolAxisEventRelative);
    connect(device, &InputDevice::tabletToolButtonEvent,
            m_tablet, &TabletInputRedirection::tabletToolButtonEvent);
    connect(device, &InputDevice::tabletPadButtonEvent,
            m_tablet, &TabletInputRedirection::tabletPadButtonEvent);
    connect(device, &InputDevice::tabletPadRingEvent,
            m_tablet, &TabletInputRedirection::tabletPadRingEvent);
    connect(device, &InputDevice::tabletPadStripEvent,
            m_tablet, &TabletInputRedirection::tabletPadStripEvent);
    connect(device, &InputDevice::tabletPadDialEvent,
            m_tablet, &TabletInputRedirection::tabletPadDialEvent);

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

void InputRedirection::addInputBackend(std::unique_ptr<InputBackend> &&inputBackend)
{
    connect(inputBackend.get(), &InputBackend::deviceAdded, this, &InputRedirection::addInputDevice);
    connect(inputBackend.get(), &InputBackend::deviceRemoved, this, &InputRedirection::removeInputDevice);

    inputBackend->setConfig(kwinApp()->inputConfig());
    inputBackend->initialize();

    m_inputBackends.push_back(std::move(inputBackend));
}

void InputRedirection::setupInputBackends()
{
    std::unique_ptr<InputBackend> inputBackend = kwinApp()->outputBackend()->createInputBackend();
    if (inputBackend) {
        addInputBackend(std::move(inputBackend));
    }
    addInputBackend(std::make_unique<FakeInputBackend>(waylandServer()->display()));
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
    const auto idleDetectors = m_idleDetectors; // the detector list can potentially change
    for (IdleDetector *idleDetector : idleDetectors) {
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
        if (effects && effects->isMouseInterception()) {
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
        if (!window->isOnCurrentActivity() || !window->isOnCurrentDesktop() || window->isMinimized() || window->isHidden() || window->isHiddenByShowDesktop()) {
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
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
#endif
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
#endif
}

void InputRedirection::registerTouchpadSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> cb)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->registerTouchpadSwipe(direction, fingerCount, action, cb);
#endif
}

void InputRedirection::registerTouchpadPinchShortcut(PinchDirection direction, uint fingerCount, QAction *onUp, std::function<void(qreal)> progressCallback)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->registerTouchpadPinch(direction, fingerCount, onUp, progressCallback);
#endif
}

void InputRedirection::registerTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->registerTouchscreenSwipe(direction, fingerCount, action, progressCallback);
#endif
}

void InputRedirection::forceRegisterTouchscreenSwipeShortcut(SwipeDirection direction, uint fingerCount, QAction *action, std::function<void(qreal)> progressCallback)
{
#if KWIN_BUILD_GLOBALSHORTCUTS
    m_shortcuts->forceRegisterTouchscreenSwipe(direction, fingerCount, action, progressCallback);
#endif
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

std::optional<QPointF> InputRedirection::implicitGrabPositionBySerial(SeatInterface *seat, uint32_t serial) const
{
    if (seat->hasImplicitPointerGrab(serial)) {
        return m_pointer->pos();
    }
    if (seat->hasImplicitTouchGrab(serial)) {
        return m_touch->position();
    }
    if (waylandServer()->tabletManagerV2()->seat(seat)->hasImplicitGrab(serial)) {
        return m_tablet->position();
    }

    return std::nullopt;
}

void InputRedirection::startInteractiveWindowSelection(std::function<void(Window *)> callback, const QByteArray &cursorName)
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
    if (m_focus.window == window) {
        return;
    }

    Window *oldFocus = m_focus.window;
    if (oldFocus) {
        disconnect(oldFocus, &Window::closed, this, &InputDeviceHandler::update);
    }

    m_focus.window = window;
    if (window) {
        connect(window, &Window::closed, this, &InputDeviceHandler::update);
    }

    focusUpdate(oldFocus, window);
}

void InputDeviceHandler::setDecoration(Decoration::DecoratedWindowImpl *decoration)
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
    Decoration::DecoratedWindowImpl *decoration = nullptr;
    auto hover = m_hover.window.data();
    if (hover && hover->decoratedWindow()) {
        if (!exclusiveContains(hover->clientGeometry(), flooredPoint(position()))) {
            // input device above decoration
            decoration = hover->decoratedWindow();
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

Decoration::DecoratedWindowImpl *InputDeviceHandler::decoration() const
{
    return m_focus.decoration;
}

} // namespace

#include "input.moc"

#include "moc_input.cpp"
