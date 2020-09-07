/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input.h"
#include "effects.h"
#include "gestures.h"
#include "globalshortcuts.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "keyboard_input.h"
#include "logind.h"
#include "main.h"
#include "pointer_input.h"
#include "tablet_input.h"
#include "touch_hide_cursor_spy.h"
#include "touch_input.h"
#include "x11client.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "internal_client.h"
#include "libinput/connection.h"
#include "libinput/device.h"
#include "platform.h"
#include "popup_input_filter.h"
#include "screenedge.h"
#include "screens.h"
#include "unmanaged.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwl/xwayland_interface.h"
#include "cursor.h"
#include <KDecoration2/Decoration>
#include <KGlobalAccel>
#include <KWaylandServer/display.h>
#include <KWaylandServer/fakeinput_interface.h>
#include <KWaylandServer/relativepointer_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/tablet_interface.h>
#include <decorations/decoratedclient.h>

//screenlocker
#include <KScreenLocker/KsldApp>
// Qt
#include <QKeyEvent>

#include <xkbcommon/xkbcommon.h>

namespace KWin
{

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

bool InputEventFilter::tabletToolButtonEvent(const QSet<uint> &pressedButtons)
{
    Q_UNUSED(pressedButtons)
    return false;
}

bool InputEventFilter::tabletPadButtonEvent(const QSet<uint> &pressedButtons)
{
    Q_UNUSED(pressedButtons)
    return false;
}

bool InputEventFilter::tabletPadStripEvent(int number, int position, bool isFinger)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    return false;
}

bool InputEventFilter::tabletPadRingEvent(int number, int position, bool isFinger)
{
    Q_UNUSED(number)
    Q_UNUSED(position)
    Q_UNUSED(isFinger)
    return false;
}

void InputEventFilter::passToWaylandServer(QKeyEvent *event)
{
    Q_ASSERT(waylandServer());
    if (event->isAutoRepeat()) {
        return;
    }
    switch (event->type()) {
    case QEvent::KeyPress:
        waylandServer()->seat()->keyPressed(event->nativeScanCode());
        break;
    case QEvent::KeyRelease:
        waylandServer()->seat()->keyReleased(event->nativeScanCode());
        break;
    default:
        break;
    }
}

class VirtualTerminalFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
        // really on press and not on release? X11 switches on press.
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            const xkb_keysym_t keysym = event->nativeVirtualKey();
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                LogindIntegration::self()->switchVirtualTerminal(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
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
                QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
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
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (event->type() == QEvent::MouseMove) {
            if (pointerSurfaceAllowed()) {
                // TODO: should the pointer position always stay in sync, i.e. not do the check?
                seat->setPointerPos(event->screenPos().toPoint());
            }
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            if (pointerSurfaceAllowed()) {
                // TODO: can we leak presses/releases here when we move the mouse in between from an allowed surface to
                //       disallowed one or vice versa?
                event->type() == QEvent::MouseButtonPress ? seat->pointerButtonPressed(nativeButton) : seat->pointerButtonReleased(nativeButton);
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
            seat->setTimestamp(event->timestamp());
            const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
            seat->pointerAxis(orientation, orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y());
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
            seat->keyPressed(event->nativeScanCode());
            break;
        case QEvent::KeyRelease:
            seat->keyReleased(event->nativeScanCode());
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
            input()->touch()->insertId(id, seat->touchDown(pos));
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
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchMove(kwaylandId, pos);
            }
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
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchUp(kwaylandId);
                input()->touch()->removeId(id);
            }
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
            c->updateMoveResize(event->screenPos().toPoint());
            break;
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                c->endMoveResize();
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
            if (c->isMove() || c->isResize()) {
                // only update if mode didn't end
                c->updateMoveResize(input()->globalPointer());
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
            c->updateMoveResize(pos.toPoint());
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
            c->endMoveResize();
            m_set = false;
            // pass through to update decoration filter later on
            return false;
        }
        m_set = false;
        return true;
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
        input()->cancelTouch();
    }
    void start(std::function<void(const QPoint &)> callback) {
        Q_ASSERT(!m_active);
        m_active = true;
        m_pointSelectionFallback = callback;
        input()->keyboard()->update();
        input()->cancelTouch();
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
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        // find client
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            auto s = qobject_cast<InternalClient *>(workspace()->findInternal(internal));
            if (s && s->isDecorated()) {
                // only perform mouse commands on decorated internal windows
                const auto actionResult = performClientMouseAction(event, s);
                if (actionResult.first) {
                    return actionResult.second;
                }
            }
            break;
        }
        default:
            break;
        }
        QMouseEvent e(event->type(),
                        event->pos() - internal->position(),
                        event->globalPos(),
                        event->button(), event->buttons(), event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        if (event->angleDelta().y() != 0) {
            auto s = qobject_cast<InternalClient *>(workspace()->findInternal(internal));
            if (s && s->isDecorated()) {
                // client window action only on vertical scrolling
                const auto actionResult = performClientWheelAction(event, s);
                if (actionResult.first) {
                    return actionResult.second;
                }
            }
        }
        const QPointF localPos = event->globalPosF() - QPointF(internal->x(), internal->y());
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta() * -1,
                        delta * -1,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool keyEvent(QKeyEvent *event) override {
        const QList<InternalClient *> &internalClients = workspace()->internalClients();
        if (internalClients.isEmpty()) {
            return false;
        }
        QWindow *found = nullptr;
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                if (!screens()->geometry().contains(w->geometry())) {
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
        } while (it != internalClients.begin());
        if (!found) {
            return false;
        }
        auto xkb = input()->keyboard()->xkb();
        Qt::Key key = xkb->toQtKey(xkb->toKeysym(event->nativeScanCode()));
        if (key == Qt::Key_Super_L || key == Qt::Key_Super_R) {
            // workaround for QTBUG-62102
            key = Qt::Key_Meta;
        }
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
            return true;
        }
        // a new touch point
        seat->setTimestamp(time);
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        touch->setInternalPressId(id);
        // Qt's touch event API is rather complex, let's do fake mouse events instead
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());

        QEnterEvent enterEvent(m_lastLocalTouchPos, m_lastLocalTouchPos, pos);
        QCoreApplication::sendEvent(internal.data(), &enterEvent);

        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        auto touch = input()->touch();
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        if (touch->internalPressId() == -1) {
            return false;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());

        QMouseEvent e(QEvent::MouseMove, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        QCoreApplication::instance()->sendEvent(internal.data(), &e);
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
        auto touch = input()->touch();
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        if (touch->internalPressId() == -1) {
            return false;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);

        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(internal.data(), &leaveEvent);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setInternalPressId(-1);
        return true;
    }
private:
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
        QCoreApplication::sendEvent(decoration.data(), &e);
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
            seat->setPointerPos(event->globalPos());
            MouseEvent *e = static_cast<MouseEvent*>(event);
            if (e->delta() != QSizeF()) {
                seat->relativePointerMotion(e->delta(), e->deltaUnaccelerated(), e->timestampMicroseconds());
            }
            break;
        }
        case QEvent::MouseButtonPress:
            seat->pointerButtonPressed(nativeButton);
            break;
        case QEvent::MouseButtonRelease:
            seat->pointerButtonReleased(nativeButton);
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
        KWaylandServer::PointerAxisSource source;
        switch (_event->axisSource()) {
        case KWin::InputRedirection::PointerAxisSourceWheel:
            source = KWaylandServer::PointerAxisSource::Wheel;
            break;
        case KWin::InputRedirection::PointerAxisSourceFinger:
            source = KWaylandServer::PointerAxisSource::Finger;
            break;
        case KWin::InputRedirection::PointerAxisSourceContinuous:
            source = KWaylandServer::PointerAxisSource::Continuous;
            break;
        case KWin::InputRedirection::PointerAxisSourceWheelTilt:
            source = KWaylandServer::PointerAxisSource::WheelTilt;
            break;
        case KWin::InputRedirection::PointerAxisSourceUnknown:
        default:
            source = KWaylandServer::PointerAxisSource::Unknown;
            break;
        }
        seat->pointerAxisV5(_event->orientation(), _event->delta(), _event->discreteDelta(), source);
        return true;
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!workspace()) {
            return false;
        }
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
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        input()->touch()->insertId(id, seat->touchDown(pos));
        return true;
    }
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchMove(kwaylandId, pos);
        }
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchUp(kwaylandId);
            input()->touch()->removeId(id);
        }
        return true;
    }
    bool pinchGestureBegin(int fingerCount, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerPinchGesture(fingerCount);
        return true;
    }
    bool pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerPinchGesture(delta, scale, angleDelta);
        return true;
    }
    bool pinchGestureEnd(quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerPinchGesture();
        return true;
    }
    bool pinchGestureCancelled(quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerPinchGesture();
        return true;
    }

    bool swipeGestureBegin(int fingerCount, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->startPointerSwipeGesture(fingerCount);
        return true;
    }
    bool swipeGestureUpdate(const QSizeF &delta, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->updatePointerSwipeGesture(delta);
        return true;
    }
    bool swipeGestureEnd(quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->endPointerSwipeGesture();
        return true;
    }
    bool swipeGestureCancelled(quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        seat->cancelPointerSwipeGesture();
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

/**
 * Handles input coming from a tablet device (e.g. wacom) often with a pen
 */
class TabletInputFilter : public QObject, public InputEventFilter
{
public:
    TabletInputFilter()
    {
    }

    static KWaylandServer::TabletSeatInterface *findTabletSeat()
    {
        auto server = waylandServer();
        if (!server) {
            return nullptr;
        }
        KWaylandServer::TabletManagerInterface *manager = server->tabletManager();
        return manager->seat(findSeat());
    }

    void integrateDevice(LibInput::Device *device)
    {
        if (device->isTabletTool()) {
            KWaylandServer::TabletSeatInterface *tabletSeat = findTabletSeat();
            if (!tabletSeat) {
                qCCritical(KWIN_CORE) << "Could not find tablet manager";
                return;
            }
            struct udev_device *const udev_device = libinput_device_get_udev_device(device->device());
            const char *devnode = udev_device_get_devnode(udev_device);
            tabletSeat->addTablet(device->vendor(), device->product(), device->sysName(), device->name(), {QString::fromUtf8(devnode)});
        }
    }
    void removeDevice(const QString &sysname)
    {
        KWaylandServer::TabletSeatInterface *tabletSeat = findTabletSeat();
        if (tabletSeat)
            tabletSeat->removeTablet(sysname);
        else
            qCCritical(KWIN_CORE) << "Could not find tablet to remove" << sysname;
    }

    bool tabletToolEvent(TabletEvent *event) override
    {
        if (!workspace()) {
            return false;
        }

        KWaylandServer::TabletSeatInterface *tabletSeat = findTabletSeat();
        if (!tabletSeat) {
            qCCritical(KWIN_CORE) << "Could not find tablet manager";
            return false;
        }
        auto tool = tabletSeat->toolByHardwareSerial(event->serialId());
        if (!tool) {
            using namespace KWaylandServer;

            const QVector<InputRedirection::Capability> capabilities = event->capabilities();
            const auto f = [](InputRedirection::Capability cap) {
                switch (cap) {
                case InputRedirection::Tilt:
                    return TabletToolInterface::Tilt;
                case InputRedirection::Pressure:
                    return TabletToolInterface::Pressure;
                case InputRedirection::Distance:
                    return TabletToolInterface::Distance;
                case InputRedirection::Rotation:
                    return TabletToolInterface::Rotation;
                case InputRedirection::Slider:
                    return TabletToolInterface::Slider;
                case InputRedirection::Wheel:
                    return TabletToolInterface::Wheel;
                }
                return TabletToolInterface::Wheel;
            };
            QVector<TabletToolInterface::Capability> ifaceCapabilities;
            ifaceCapabilities.resize(capabilities.size());
            std::transform(capabilities.constBegin(), capabilities.constEnd(), ifaceCapabilities.begin(), f);

            TabletToolInterface::Type toolType = TabletToolInterface::Type::Pen;
            switch (event->toolType()) {
            case InputRedirection::Pen:
                toolType = TabletToolInterface::Type::Pen;
                break;
            case InputRedirection::Eraser:
                toolType = TabletToolInterface::Type::Eraser;
                break;
            case InputRedirection::Brush:
                toolType = TabletToolInterface::Type::Brush;
                break;
            case InputRedirection::Pencil:
                toolType = TabletToolInterface::Type::Pencil;
                break;
            case InputRedirection::Airbrush:
                toolType = TabletToolInterface::Type::Airbrush;
                break;
            case InputRedirection::Finger:
                toolType = TabletToolInterface::Type::Finger;
                break;
            case InputRedirection::Mouse:
                toolType = TabletToolInterface::Type::Mouse;
                break;
            case InputRedirection::Lens:
                toolType = TabletToolInterface::Type::Lens;
                break;
            case InputRedirection::Totem:
                toolType = TabletToolInterface::Type::Totem;
                break;
            }
            tool = tabletSeat->addTool(toolType, event->serialId(), event->uniqueId(), ifaceCapabilities);

            const auto cursor = new Cursor(tool);
            Cursors::self()->addCursor(cursor);
            m_cursorByTool[tool] = cursor;

            connect(tool, &TabletToolInterface::cursorChanged, cursor, &Cursor::cursorChanged);
            connect(tool, &TabletToolInterface::cursorChanged, cursor, [cursor] (TabletCursor* tcursor) {
                static const auto createDefaultCursor = [] {
                    WaylandCursorImage defaultCursor;
                    WaylandCursorImage::Image ret;
                    defaultCursor.loadThemeCursor(CursorShape(Qt::CrossCursor), &ret);
                    return ret;
                };
                static const auto defaultCursor = createDefaultCursor();
                if (!tcursor) {
                    cursor->updateCursor(defaultCursor.image, defaultCursor.hotspot);
                    return;
                }
                auto cursorSurface = tcursor->surface();
                if (!cursorSurface) {
                    cursor->updateCursor(defaultCursor.image, defaultCursor.hotspot);
                    return;
                }
                auto buffer = cursorSurface->buffer();
                if (!buffer) {
                    cursor->updateCursor(defaultCursor.image, defaultCursor.hotspot);
                    return;
                }

                QImage cursorImage;
                cursorImage = buffer->data().copy();
                cursorImage.setDevicePixelRatio(cursorSurface->bufferScale());

                cursor->updateCursor(cursorImage, tcursor->hotspot());
            });
            emit cursor->cursorChanged();
        }

        KWaylandServer::TabletInterface *tablet = tabletSeat->tabletByName(event->tabletSysName());

        Toplevel *toplevel = input()->findToplevel(event->globalPos());
        if (!toplevel || !toplevel->surface()) {
            return false;
        }

        KWaylandServer::SurfaceInterface *surface = toplevel->surface();
        tool->setCurrentSurface(surface);

        if (!tool->isClientSupported() || !tablet->isSurfaceSupported(surface)) {
            return emulateTabletEvent(event);
        }

        switch (event->type()) {
        case QEvent::TabletMove: {
            const auto pos = event->globalPosF() - toplevel->bufferGeometry().topLeft();
            tool->sendMotion(pos);
            m_cursorByTool[tool]->setPos(event->globalPos());
            break;
        } case QEvent::TabletEnterProximity: {
            tool->sendProximityIn(tablet);
            break;
        } case QEvent::TabletLeaveProximity:
            tool->sendProximityOut();
            break;
        case QEvent::TabletPress: {
            const auto pos = event->globalPosF() - toplevel->bufferGeometry().topLeft();
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
        waylandServer()->simulateUserActivity();
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
            input()->pointer()->processMotion(event->globalPosF(), event->timestamp());
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
        waylandServer()->simulateUserActivity();
        return true;
    }
    QHash<KWaylandServer::TabletToolInterface*, Cursor*> m_cursorByTool;
};

class DragAndDropInputFilter : public InputEventFilter
{
public:
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
            seat->setPointerPos(pos);

            const auto eventPos = event->globalPos();
            // TODO: use InputDeviceHandler::at() here and check isClient()?
            Toplevel *t = input()->findManagedToplevel(eventPos);
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
                    if (AbstractClient *c = qobject_cast<AbstractClient*>(t)) {
                        workspace()->activateClient(c);
                    }
                    seat->setDragTarget(t->surface(), t->inputTransformation());
                }
            } else {
                // no window at that place, if we have a surface we need to reset
                seat->setDragTarget(nullptr);
            }
            break;
        }
        case QEvent::MouseButtonPress:
            seat->pointerButtonPressed(nativeButton);
            break;
        case QEvent::MouseButtonRelease:
            seat->pointerButtonReleased(nativeButton);
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
        input()->touch()->insertId(id, seat->touchDown(pos));
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
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId == -1) {
            return true;
        }

        seat->touchMove(kwaylandId, pos);

        if (Toplevel *t = input()->findToplevel(pos.toPoint())) {
            // TODO: consider decorations
            if (t->surface() != seat->dragSurface()) {
                if (AbstractClient *c = qobject_cast<AbstractClient*>(t)) {
                    workspace()->activateClient(c);
                }
                seat->setDragTarget(t->surface(), pos, t->inputTransformation());
            }
        } else {
            // no window at that place, if we have a surface we need to reset
            seat->setDragTarget(nullptr);
        }
        return true;
    }
    bool touchUp(qint32 id, quint32 time) override {
        auto seat = waylandServer()->seat();
        if (!seat->isDragTouch()) {
            return false;
        }
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchUp(kwaylandId);
            input()->touch()->removeId(id);
        }
        if (m_touchId == id) {
            m_touchId = -1;
        }
        return true;
    }
private:
    qint32 m_touchId = -1;
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
    if (Application::usesLibinput()) {
        if (LogindIntegration::self()->hasSessionControl()) {
            setupLibInput();
        } else {
            LibInput::Connection::createThread();
            if (LogindIntegration::self()->isConnected()) {
                LogindIntegration::self()->takeControl();
            } else {
                connect(LogindIntegration::self(), &LogindIntegration::connectedChanged, LogindIntegration::self(), &LogindIntegration::takeControl);
            }
            connect(LogindIntegration::self(), &LogindIntegration::hasSessionControlChanged, this,
                [this] (bool sessionControl) {
                    if (sessionControl) {
                        setupLibInput();
                    }
                }
            );
        }
    }
    connect(kwinApp(), &Application::workspaceCreated, this, &InputRedirection::setupWorkspace);
    reconfigure();
}

InputRedirection::~InputRedirection()
{
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
    m_shortcuts->init();
}

void InputRedirection::setupWorkspace()
{
    if (waylandServer()) {
        using namespace KWaylandServer;
        FakeInputInterface *fakeInput = waylandServer()->display()->createFakeInput(this);
        fakeInput->create();
        connect(fakeInput, &FakeInputInterface::deviceCreated, this,
            [this] (FakeInputDevice *device) {
                connect(device, &FakeInputDevice::authenticationRequested, this,
                    [device] (const QString &application, const QString &reason) {
                        Q_UNUSED(application)
                        Q_UNUSED(reason)
                        // TODO: make secure
                        device->setAuthentication(true);
                    }
                );
                connect(device, &FakeInputDevice::pointerMotionRequested, this,
                    [this] (const QSizeF &delta) {
                        // TODO: Fix time
                        m_pointer->processMotion(globalPointer() + QPointF(delta.width(), delta.height()), 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
               connect(device, &FakeInputDevice::pointerMotionAbsoluteRequested, this,
                    [this] (const QPointF &pos) {
                        // TODO: Fix time
                        m_pointer->processMotion(pos, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonPressRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonPressed, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonReleaseRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonReleased, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::pointerAxisRequested, this,
                    [this] (Qt::Orientation orientation, qreal delta) {
                        // TODO: Fix time
                        InputRedirection::PointerAxis axis;
                        switch (orientation) {
                        case Qt::Horizontal:
                            axis = InputRedirection::PointerAxisHorizontal;
                            break;
                        case Qt::Vertical:
                            axis = InputRedirection::PointerAxisVertical;
                            break;
                        default:
                            Q_UNREACHABLE();
                            break;
                        }
                        // TODO: Fix time
                        m_pointer->processAxis(axis, delta, 0, InputRedirection::PointerAxisSourceUnknown, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::touchDownRequested, this,
                   [this] (qint32 id, const QPointF &pos) {
                       // TODO: Fix time
                       m_touch->processDown(id, pos, 0);
                        waylandServer()->simulateUserActivity();
                   }
                );
                connect(device, &FakeInputDevice::touchMotionRequested, this,
                   [this] (qint32 id, const QPointF &pos) {
                       // TODO: Fix time
                       m_touch->processMotion(id, pos, 0);
                        waylandServer()->simulateUserActivity();
                   }
                );
                connect(device, &FakeInputDevice::touchUpRequested, this,
                    [this] (qint32 id) {
                        // TODO: Fix time
                        m_touch->processUp(id, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::touchCancelRequested, this,
                    [this] () {
                        m_touch->cancel();
                    }
                );
                connect(device, &FakeInputDevice::touchFrameRequested, this,
                   [this] () {
                       m_touch->frame();
                   }
                );
                connect(device, &FakeInputDevice::keyboardKeyPressRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_keyboard->processKey(button, InputRedirection::KeyboardKeyPressed, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
                connect(device, &FakeInputDevice::keyboardKeyReleaseRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_keyboard->processKey(button, InputRedirection::KeyboardKeyReleased, 0);
                        waylandServer()->simulateUserActivity();
                    }
                );
            }
        );
        connect(workspace(), &Workspace::configChanged, this, &InputRedirection::reconfigure);

        m_keyboard->init();
        m_pointer->init();
        m_touch->init();
        m_tablet->init();
    }
    setupInputFilters();
}

void InputRedirection::setupInputFilters()
{
    const bool hasGlobalShortcutSupport = !waylandServer() || waylandServer()->hasGlobalShortcutSupport();
    if (LogindIntegration::self()->hasSessionControl() && hasGlobalShortcutSupport) {
        installInputEventFilter(new VirtualTerminalFilter);
    }
    if (waylandServer()) {
        installInputEventSpy(new TouchHideCursorSpy);
        if (hasGlobalShortcutSupport) {
            installInputEventFilter(new TerminateServerFilter);
        }
        installInputEventFilter(new DragAndDropInputFilter);
        installInputEventFilter(new LockScreenFilter);
        installInputEventFilter(new PopupInputFilter);
        m_windowSelector = new WindowSelectorFilter;
        installInputEventFilter(m_windowSelector);
    }
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
    installInputEventFilter(new DecorationEventFilter);
    installInputEventFilter(new InternalWindowEventFilter);
    if (waylandServer()) {
        installInputEventFilter(new WindowActionInputFilter);
        installInputEventFilter(new ForwardInputFilter);

        if (m_libInput) {
            m_tabletSupport = new TabletInputFilter;
            for (LibInput::Device *dev : m_libInput->devices()) {
                m_tabletSupport->integrateDevice(dev);
            }
            connect(m_libInput, &LibInput::Connection::deviceAdded, m_tabletSupport, &TabletInputFilter::integrateDevice);
            connect(m_libInput, &LibInput::Connection::deviceRemovedSysName, m_tabletSupport, &TabletInputFilter::removeDevice);
            installInputEventFilter(m_tabletSupport);
        }
    }
}

void InputRedirection::reconfigure()
{
    if (Application::usesLibinput()) {
        auto inputConfig = InputConfig::self()->inputConfig();
        inputConfig->reparseConfiguration();
        const auto config = inputConfig->group(QStringLiteral("Keyboard"));
        const int delay = config.readEntry("RepeatDelay", 660);
        const int rate = config.readEntry("RepeatRate", 25);
        const bool enabled = config.readEntry("KeyboardRepeating", 0) == 0;

        waylandServer()->seat()->setKeyRepeatInfo(enabled ? rate : 0, delay);
    }
}

void InputRedirection::setupLibInput()
{
    if (!Application::usesLibinput()) {
        return;
    }
    if (m_libInput) {
        return;
    }
    LibInput::Connection *conn = LibInput::Connection::create(this);
    m_libInput = conn;
    if (conn) {

        if (waylandServer()) {
            // create relative pointer manager
            waylandServer()->display()->createRelativePointerManager(KWaylandServer::RelativePointerInterfaceVersion::UnstableV1, waylandServer()->display())->create();
        }

        conn->setInputConfig(InputConfig::self()->inputConfig());
        conn->updateLEDs(m_keyboard->xkb()->leds());
        waylandServer()->updateKeyState(m_keyboard->xkb()->leds());
        connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, waylandServer(), &WaylandServer::updateKeyState);
        connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, conn, &LibInput::Connection::updateLEDs);
        connect(conn, &LibInput::Connection::eventsRead, this,
            [this] {
                m_libInput->processEvents();
            }, Qt::QueuedConnection
        );
        conn->setup();
        connect(conn, &LibInput::Connection::pointerButtonChanged, m_pointer, &PointerInputRedirection::processButton);
        connect(conn, &LibInput::Connection::pointerAxisChanged, m_pointer, &PointerInputRedirection::processAxis);
        connect(conn, &LibInput::Connection::pinchGestureBegin, m_pointer, &PointerInputRedirection::processPinchGestureBegin);
        connect(conn, &LibInput::Connection::pinchGestureUpdate, m_pointer, &PointerInputRedirection::processPinchGestureUpdate);
        connect(conn, &LibInput::Connection::pinchGestureEnd, m_pointer, &PointerInputRedirection::processPinchGestureEnd);
        connect(conn, &LibInput::Connection::pinchGestureCancelled, m_pointer, &PointerInputRedirection::processPinchGestureCancelled);
        connect(conn, &LibInput::Connection::swipeGestureBegin, m_pointer, &PointerInputRedirection::processSwipeGestureBegin);
        connect(conn, &LibInput::Connection::swipeGestureUpdate, m_pointer, &PointerInputRedirection::processSwipeGestureUpdate);
        connect(conn, &LibInput::Connection::swipeGestureEnd, m_pointer, &PointerInputRedirection::processSwipeGestureEnd);
        connect(conn, &LibInput::Connection::swipeGestureCancelled, m_pointer, &PointerInputRedirection::processSwipeGestureCancelled);
        connect(conn, &LibInput::Connection::keyChanged, m_keyboard, &KeyboardInputRedirection::processKey);
        connect(conn, &LibInput::Connection::pointerMotion, this,
            [this] (const QSizeF &delta, const QSizeF &deltaNonAccel, uint32_t time, quint64 timeMicroseconds, LibInput::Device *device) {
                m_pointer->processMotion(m_pointer->pos() + QPointF(delta.width(), delta.height()), delta, deltaNonAccel, time, timeMicroseconds, device);
            }
        );
        connect(conn, &LibInput::Connection::pointerMotionAbsolute, this,
            [this] (QPointF orig, QPointF screen, uint32_t time, LibInput::Device *device) {
                Q_UNUSED(orig)
                m_pointer->processMotion(screen, time, device);
            }
        );
        connect(conn, &LibInput::Connection::touchDown, m_touch, &TouchInputRedirection::processDown);
        connect(conn, &LibInput::Connection::touchUp, m_touch, &TouchInputRedirection::processUp);
        connect(conn, &LibInput::Connection::touchMotion, m_touch, &TouchInputRedirection::processMotion);
        connect(conn, &LibInput::Connection::touchCanceled, m_touch, &TouchInputRedirection::cancel);
        connect(conn, &LibInput::Connection::touchFrame, m_touch, &TouchInputRedirection::frame);
        auto handleSwitchEvent = [this] (SwitchEvent::State state, quint32 time, quint64 timeMicroseconds, LibInput::Device *device) {
            SwitchEvent event(state, time, timeMicroseconds, device);
            processSpies(std::bind(&InputEventSpy::switchEvent, std::placeholders::_1, &event));
            processFilters(std::bind(&InputEventFilter::switchEvent, std::placeholders::_1, &event));
        };
        connect(conn, &LibInput::Connection::switchToggledOn, this,
                std::bind(handleSwitchEvent, SwitchEvent::State::On, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        connect(conn, &LibInput::Connection::switchToggledOff, this,
                std::bind(handleSwitchEvent, SwitchEvent::State::Off, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        connect(conn, &LibInput::Connection::tabletToolEvent,
                m_tablet, &TabletInputRedirection::tabletToolEvent);
        connect(conn, &LibInput::Connection::tabletToolButtonEvent,
                m_tablet, &TabletInputRedirection::tabletToolButtonEvent);
        connect(conn, &LibInput::Connection::tabletPadButtonEvent,
                m_tablet, &TabletInputRedirection::tabletPadButtonEvent);
        connect(conn, &LibInput::Connection::tabletPadRingEvent,
                m_tablet, &TabletInputRedirection::tabletPadRingEvent);
        connect(conn, &LibInput::Connection::tabletPadStripEvent,
                m_tablet, &TabletInputRedirection::tabletPadStripEvent);

        if (screens()) {
            setupLibInputWithScreens();
        } else {
            connect(kwinApp(), &Application::screensCreated, this, &InputRedirection::setupLibInputWithScreens);
        }
        if (auto s = findSeat()) {
            // Workaround for QTBUG-54371: if there is no real keyboard Qt doesn't request virtual keyboard
            s->setHasKeyboard(true);
            s->setHasPointer(conn->hasPointer());
            s->setHasTouch(conn->hasTouch());
            connect(conn, &LibInput::Connection::hasAlphaNumericKeyboardChanged, this,
                [this] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    // TODO: this should update the seat, only workaround for QTBUG-54371
                    emit hasAlphaNumericKeyboardChanged(set);
                }
            );
            connect(conn, &LibInput::Connection::hasTabletModeSwitchChanged, this,
                [this] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    emit hasTabletModeSwitchChanged(set);
                }
            );
            connect(conn, &LibInput::Connection::hasPointerChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasPointer(set);
                }
            );
            connect(conn, &LibInput::Connection::hasTouchChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasTouch(set);
                }
            );
        }
        connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, m_libInput,
            [this] (bool active) {
                if (!active) {
                    m_libInput->deactivate();
                }
            }
        );
    }

    setupTouchpadShortcuts();
}

void InputRedirection::setupTouchpadShortcuts()
{
    if (!m_libInput) {
        return;
    }
    QAction *touchpadToggleAction = new QAction(this);
    QAction *touchpadOnAction = new QAction(this);
    QAction *touchpadOffAction = new QAction(this);

    touchpadToggleAction->setObjectName(QStringLiteral("Toggle Touchpad"));
    touchpadToggleAction->setProperty("componentName", s_touchpadComponent);
    touchpadOnAction->setObjectName(QStringLiteral("Enable Touchpad"));
    touchpadOnAction->setProperty("componentName", s_touchpadComponent);
    touchpadOffAction->setObjectName(QStringLiteral("Disable Touchpad"));
    touchpadOffAction->setProperty("componentName", s_touchpadComponent);
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
    connect(touchpadToggleAction, &QAction::triggered, m_libInput, &LibInput::Connection::toggleTouchpads);
    connect(touchpadOnAction, &QAction::triggered, m_libInput, &LibInput::Connection::enableTouchpads);
    connect(touchpadOffAction, &QAction::triggered, m_libInput, &LibInput::Connection::disableTouchpads);
}

bool InputRedirection::hasAlphaNumericKeyboard()
{
    if (m_libInput) {
        return m_libInput->hasAlphaNumericKeyboard();
    }
    return true;
}

bool InputRedirection::hasTabletModeSwitch()
{
    if (m_libInput) {
        return m_libInput->hasTabletModeSwitch();
    }
    return false;
}

void InputRedirection::setupLibInputWithScreens()
{
    if (!screens() || !m_libInput) {
        return;
    }
    m_libInput->setScreenSize(screens()->size());
    m_libInput->updateScreens();
    connect(screens(), &Screens::sizeChanged, this,
        [this] {
            m_libInput->setScreenSize(screens()->size());
        }
    );
    connect(screens(), &Screens::changed, m_libInput, &LibInput::Connection::updateScreens);
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    m_pointer->processMotion(pos, time);
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    m_pointer->processButton(button, state, time);
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, qint32 discreteDelta, PointerAxisSource source, uint32_t time)
{
    m_pointer->processAxis(axis, delta, discreteDelta, source, time);
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    m_keyboard->processKey(key, state, time);
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    m_keyboard->processModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    m_keyboard->processKeymapChange(fd, size);
}

void InputRedirection::processTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    m_touch->processDown(id, pos, time);
}

void InputRedirection::processTouchUp(qint32 id, quint32 time)
{
    m_touch->processUp(id, time);
}

void InputRedirection::processTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    m_touch->processMotion(id, pos, time);
}

void InputRedirection::cancelTouch()
{
    m_touch->cancel();
}

void InputRedirection::touchFrame()
{
    m_touch->frame();
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    return m_pointer->buttons();
}

static bool acceptsInput(Toplevel *t, const QPoint &pos)
{
    const QRegion input = t->inputShape();
    if (input.isEmpty()) {
        return true;
    }
    // TODO: What about sub-surfaces sticking outside the main surface?
    const QPoint localPoint = pos - t->bufferGeometry().topLeft();
    return input.contains(localPoint);
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
        foreach (Unmanaged *u, unmanaged) {
            if (u->inputGeometry().contains(pos) && acceptsInput(u, pos)) {
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
        if (t->inputGeometry().contains(pos) && acceptsInput(t, pos)) {
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

bool InputDeviceHandler::setAt(Toplevel *toplevel)
{
    if (m_at.at == toplevel) {
        return false;
    }
    auto old = m_at.at;
    disconnect(m_at.surfaceCreatedConnection);
    m_at.surfaceCreatedConnection = QMetaObject::Connection();

    m_at.at = toplevel;
    emit atChanged(old, toplevel);
    return true;
}

void InputDeviceHandler::setFocus(Toplevel *toplevel)
{
    m_focus.focus = toplevel;
    //TODO: call focusUpdate?
}

void InputDeviceHandler::setDecoration(QPointer<Decoration::DecoratedClientImpl> decoration)
{
    auto oldDeco = m_focus.decoration;
    m_focus.decoration = decoration;
    cleanupDecoration(oldDeco.data(), m_focus.decoration.data());
    emit decorationChanged();
}

void InputDeviceHandler::setInternalWindow(QWindow *window)
{
    m_focus.internalWindow = window;
    //TODO: call internalWindowUpdate?
}

void InputDeviceHandler::updateFocus()
{
    auto oldFocus = m_focus.focus;

    if (m_at.at && !m_at.at->surface()) {
        // The surface has not yet been created (special XWayland case).
        // Therefore listen for its creation.
        if (!m_at.surfaceCreatedConnection) {
            m_at.surfaceCreatedConnection = connect(m_at.at, &Toplevel::surfaceChanged,
                                                    this, &InputDeviceHandler::update);
        }
        m_focus.focus = nullptr;
    } else {
        m_focus.focus = m_at.at;
    }

    focusUpdate(oldFocus, m_focus.focus);
}

bool InputDeviceHandler::updateDecoration()
{
    const auto oldDeco = m_focus.decoration;
    m_focus.decoration = nullptr;

    auto *ac = qobject_cast<AbstractClient*>(m_at.at);
    if (ac && ac->decoratedClient()) {
        const QRect clientRect = QRect(ac->clientPos(), ac->clientSize()).translated(ac->pos());
        if (!clientRect.contains(position().toPoint())) {
            // input device above decoration
            m_focus.decoration = ac->decoratedClient();
        }
    }

    if (m_focus.decoration == oldDeco) {
        // no change to decoration
        return false;
    }
    cleanupDecoration(oldDeco.data(), m_focus.decoration.data());
    emit decorationChanged();
    return true;
}

void InputDeviceHandler::updateInternalWindow(QWindow *window)
{
    if (m_focus.internalWindow == window) {
        // no change
        return;
    }
    const auto oldInternal = m_focus.internalWindow;
    m_focus.internalWindow = window;
    cleanupInternalWindow(oldInternal, window);
}

void InputDeviceHandler::update()
{
    if (!m_inited) {
        return;
    }

    Toplevel *toplevel = nullptr;
    QWindow *internalWindow = nullptr;

    if (!positionValid()) {
        const auto pos = position().toPoint();
        internalWindow = findInternalWindow(pos);
        if (internalWindow) {
            toplevel = workspace()->findInternal(internalWindow);
        } else {
            toplevel = input()->findToplevel(pos);
        }
    }
    // Always set the toplevel at the position of the input device.
    setAt(toplevel);

    if (focusUpdatesBlocked()) {
        return;
    }

    if (internalWindow) {
        if (m_focus.internalWindow != internalWindow) {
            // changed internal window
            updateDecoration();
            updateInternalWindow(internalWindow);
            updateFocus();
        } else if (updateDecoration()) {
            // went onto or off from decoration, update focus
            updateFocus();
        }
        return;
    }
    updateInternalWindow(nullptr);

    if (m_focus.focus != m_at.at) {
        // focus change
        updateDecoration();
        updateFocus();
        return;
    }
    // check if switched to/from decoration while staying on the same Toplevel
    if (updateDecoration()) {
        // went onto or off from decoration, update focus
        updateFocus();
    }
}

Toplevel *InputDeviceHandler::at() const
{
    return m_at.at.data();
}

Toplevel *InputDeviceHandler::focus() const
{
    return m_focus.focus.data();
}

QWindow* InputDeviceHandler::findInternalWindow(const QPoint &pos) const
{
    if (waylandServer()->isScreenLocked()) {
        return nullptr;
    }

    const QList<InternalClient *> &internalClients = workspace()->internalClients();
    if (internalClients.isEmpty()) {
        return nullptr;
    }

    auto it = internalClients.end();
    do {
        --it;
        QWindow *w = (*it)->internalWindow();
        if (!w || !w->isVisible()) {
            continue;
        }
        if (!(*it)->frameGeometry().contains(pos)) {
            continue;
        }
        // check input mask
        const QRegion mask = w->mask().translated(w->geometry().topLeft());
        if (!mask.isEmpty() && !mask.contains(pos)) {
            continue;
        }
        if (w->property("outputOnly").toBool()) {
            continue;
        }
        return w;
    } while (it != internalClients.begin());

    return nullptr;
}

} // namespace
