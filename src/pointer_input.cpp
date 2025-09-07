/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "pointer_input.h"

#include "config-kwin.h"

#include "core/output.h"
#include "cursorsource.h"
#include "decorations/decoratedwindow.h"
#include "effect/effecthandler.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "mousebuttons.h"
#include "osd.h"
#include "screenedge.h"
#include "wayland/abstract_data_source.h"
#include "wayland/display.h"
#include "wayland/pointer.h"
#include "wayland/pointerconstraints_v1.h"
#include "wayland/pointerwarp_v1.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
// KDecoration
#include <KDecoration3/Decoration>
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

#include <KLocalizedString>

#include <QHoverEvent>
#include <QPainter>
#include <QWindow>

#include <linux/input.h>

#include <cmath>

namespace KWin
{

static bool screenContainsPos(const QPointF &pos)
{
    const auto outputs = workspace()->outputs();
    for (const Output *output : outputs) {
        if (output->geometry().contains(flooredPoint(pos))) {
            return true;
        }
    }
    return false;
}

static QPointF confineToBoundingBox(const QPointF &pos, const QRectF &boundingBox)
{
    return QPointF(
        std::clamp(pos.x(), boundingBox.left(), boundingBox.right() - 1.0),
        std::clamp(pos.y(), boundingBox.top(), boundingBox.bottom() - 1.0));
}

PointerInputRedirection::PointerInputRedirection(InputRedirection *parent)
    : InputDeviceHandler(parent)
    , m_cursor(nullptr)
{
}

PointerInputRedirection::~PointerInputRedirection() = default;

CursorTheme PointerInputRedirection::cursorTheme() const
{
    return m_cursor->theme();
}

void PointerInputRedirection::init()
{
    Q_ASSERT(!inited());
    waylandServer()->seat()->setHasPointer(input()->hasPointer());
    connect(input(), &InputRedirection::hasPointerChanged,
            waylandServer()->seat(), &SeatInterface::setHasPointer);

    m_cursor = new CursorImage(this);
    setInited(true);
    InputDeviceHandler::init();

    if (!input()->hasPointer()) {
        Cursors::self()->hideCursor();
    }
    connect(input(), &InputRedirection::hasPointerChanged, this, []() {
        if (input()->hasPointer()) {
            Cursors::self()->showCursor();
        } else {
            Cursors::self()->hideCursor();
        }
    });

    connect(m_cursor, &CursorImage::changed, Cursors::self()->mouse(), [this] {
        Cursors::self()->mouse()->setSource(m_cursor->source());
        m_cursor->updateCursorOutputs(m_pos);
    });
    Q_EMIT m_cursor->changed();

    connect(workspace(), &Workspace::outputsChanged, this, &PointerInputRedirection::updateAfterScreenChange);
#if KWIN_BUILD_SCREENLOCKER
    if (kwinApp()->supportsLockScreen()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, [this]() {
            if (waylandServer()->seat()->hasPointer()) {
                waylandServer()->seat()->cancelPointerPinchGesture();
                waylandServer()->seat()->cancelPointerSwipeGesture();
            }
            update();
        });
    }
#endif
    connect(workspace(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
    connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, [this]() {
        // need to force a focused pointer change
        setFocus(nullptr);
        update();
    });
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this](Window *window) {
        connect(window, &Window::interactiveMoveResizeStarted, this, &PointerInputRedirection::updateOnStartMoveResize);
        connect(window, &Window::interactiveMoveResizeFinished, this, &PointerInputRedirection::update);
    };
    const auto clients = workspace()->windows();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::windowAdded, this, setupMoveResizeConnection);

    // warp the cursor to center of screen containing the workspace center
    if (const Output *output = workspace()->outputAt(workspace()->geometry().center())) {
        warp(output->geometry().center());
    }
    updateAfterScreenChange();

    connect(waylandServer()->pointerWarp(), &PointerWarpV1::warpRequested, this, [](SurfaceInterface *surface, PointerInterface *pointer, const QPointF &point, uint32_t serial) {
        if (serial != waylandServer()->seat()->pointer()->focusedSerial()) {
            return;
        }
        if (!surface->boundingRect().contains(point)) {
            return;
        }
        Window *window = waylandServer()->findWindow(surface->mainSurface());
        if (!window) {
            return;
        }
        input()->pointer()->warp(window->mapFromLocal(surface->mapToMainSurface(point)));
    });
}

void PointerInputRedirection::updateOnStartMoveResize()
{
    breakPointerConstraints(focus() ? focus()->surface() : nullptr);
    disconnectPointerConstraintsConnection();
    setFocus(nullptr);
}

void PointerInputRedirection::updateToReset()
{
    if (decoration()) {
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
        setDecoration(nullptr);
    }
    if (focus()) {
        if (focus()->isClient()) {
            focus()->pointerLeaveEvent();
        }
        disconnect(m_focusGeometryConnection);
        m_focusGeometryConnection = QMetaObject::Connection();
        breakPointerConstraints(focus()->surface());
        disconnectPointerConstraintsConnection();
        setFocus(nullptr);
    }
}

class PositionUpdateBlocker
{
public:
    PositionUpdateBlocker(PointerInputRedirection *pointer)
        : m_pointer(pointer)
    {
        s_counter++;
    }
    ~PositionUpdateBlocker()
    {
        s_counter--;
        if (s_counter == 0) {
            if (!s_scheduledPositions.isEmpty()) {
                const auto pos = s_scheduledPositions.takeFirst();
                m_pointer->processMotionInternal(pos.pos, pos.delta, pos.deltaNonAccelerated, pos.time, nullptr, pos.type);
            }
        }
    }

    static bool isPositionBlocked()
    {
        return s_counter > 0;
    }

    static void schedulePosition(const QPointF &pos, const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, PointerInputRedirection::MotionType type)
    {
        s_scheduledPositions.append({pos, delta, deltaNonAccelerated, time, type});
    }

private:
    static int s_counter;
    struct ScheduledPosition
    {
        QPointF pos;
        QPointF delta;
        QPointF deltaNonAccelerated;
        std::chrono::microseconds time;
        PointerInputRedirection::MotionType type;
    };
    static QList<ScheduledPosition> s_scheduledPositions;

    PointerInputRedirection *m_pointer;
};

int PositionUpdateBlocker::s_counter = 0;
QList<PositionUpdateBlocker::ScheduledPosition> PositionUpdateBlocker::s_scheduledPositions;

void PointerInputRedirection::processMotionAbsolute(const QPointF &pos, std::chrono::microseconds time, InputDevice *device)
{
    processMotionInternal(pos, QPointF(), QPointF(), time, device, MotionType::Motion);
}

void PointerInputRedirection::processWarp(const QPointF &pos, std::chrono::microseconds time, InputDevice *device)
{
    processMotionInternal(pos, QPointF(), QPointF(), time, device, MotionType::Warp);
}

void PointerInputRedirection::processMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device)
{
    processMotionInternal(m_pos + delta, delta, deltaNonAccelerated, time, device, MotionType::Motion);
}

void PointerInputRedirection::processMotionInternal(const QPointF &pos, const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device, MotionType type)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    if (PositionUpdateBlocker::isPositionBlocked()) {
        PositionUpdateBlocker::schedulePosition(pos, delta, deltaNonAccelerated, time, type);
        return;
    }

    PositionUpdateBlocker blocker(this);
    updatePosition(pos, time);

    PointerMotionEvent event{
        .device = device,
        .position = m_pos,
        .delta = delta,
        .deltaUnaccelerated = deltaNonAccelerated,
        .warp = type == MotionType::Warp,
        .buttons = m_qtButtons,
        .modifiers = input()->keyboardModifiers(),
        .modifiersRelevantForShortcuts = input()->modifiersRelevantForGlobalShortcuts(),
        .timestamp = time,
    };

    update();
    input()->processSpies(&InputEventSpy::pointerMotion, &event);
    input()->processFilters(&InputEventFilter::pointerMotion, &event);
}

void PointerInputRedirection::processButton(uint32_t button, PointerButtonState state, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }

    if (state == PointerButtonState::Pressed) {
        update();
    }

    updateButton(button, state);

    PointerButtonEvent event{
        .device = device,
        .position = m_pos,
        .state = state,
        .button = buttonToQtMouseButton(button),
        .nativeButton = button,
        .buttons = m_qtButtons,
        .modifiers = input()->keyboardModifiers(),
        .modifiersRelevantForShortcuts = input()->modifiersRelevantForGlobalShortcuts(),
        .timestamp = time,
    };

    input()->processSpies(&InputEventSpy::pointerButton, &event);
    input()->processFilters(&InputEventFilter::pointerButton, &event);
    if (state == PointerButtonState::Pressed) {
        input()->setLastInputSerial(waylandServer()->seat()->display()->serial());
        if (auto f = focus()) {
            f->setLastUsageSerial(waylandServer()->seat()->display()->serial());
        }
    }

    if (state == PointerButtonState::Released) {
        update();
    }
}

void PointerInputRedirection::processAxis(PointerAxis axis, qreal delta, qint32 deltaV120,
                                          PointerAxisSource source, bool inverted, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }

    update();

    Q_EMIT input()->pointerAxisChanged(axis, delta);

    PointerAxisEvent event{
        .device = device,
        .position = m_pos,
        .delta = delta,
        .deltaV120 = deltaV120,
        .orientation = (axis == PointerAxis::Horizontal) ? Qt::Horizontal : Qt::Vertical,
        .source = source,
        .buttons = m_qtButtons,
        .modifiers = input()->keyboardModifiers(),
        .modifiersRelevantForGlobalShortcuts = input()->modifiersRelevantForGlobalShortcuts(),
        .inverted = inverted,
        .timestamp = time,
    };

    input()->processSpies(&InputEventSpy::pointerAxis, &event);
    input()->processFilters(&InputEventFilter::pointerAxis, &event);
}

void PointerInputRedirection::processSwipeGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerSwipeGestureBeginEvent event{
        .fingerCount = fingerCount,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::swipeGestureBegin, &event);
    input()->processFilters(&InputEventFilter::swipeGestureBegin, &event);
}

void PointerInputRedirection::processSwipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerSwipeGestureUpdateEvent event{
        .delta = delta,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::swipeGestureUpdate, &event);
    input()->processFilters(&InputEventFilter::swipeGestureUpdate, &event);
}

void PointerInputRedirection::processSwipeGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerSwipeGestureEndEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::swipeGestureEnd, &event);
    input()->processFilters(&InputEventFilter::swipeGestureEnd, &event);
}

void PointerInputRedirection::processSwipeGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerSwipeGestureCancelEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::swipeGestureCancelled, &event);
    input()->processFilters(&InputEventFilter::swipeGestureCancelled, &event);
}

void PointerInputRedirection::processPinchGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerPinchGestureBeginEvent event{
        .fingerCount = fingerCount,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::pinchGestureBegin, &event);
    input()->processFilters(&InputEventFilter::pinchGestureBegin, &event);
}

void PointerInputRedirection::processPinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerPinchGestureUpdateEvent event{
        .scale = scale,
        .angleDelta = angleDelta,
        .delta = delta,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::pinchGestureUpdate, &event);
    input()->processFilters(&InputEventFilter::pinchGestureUpdate, &event);
}

void PointerInputRedirection::processPinchGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerPinchGestureEndEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::pinchGestureEnd, &event);
    input()->processFilters(&InputEventFilter::pinchGestureEnd, &event);
}

void PointerInputRedirection::processPinchGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    PointerPinchGestureCancelEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::pinchGestureCancelled, &event);
    input()->processFilters(&InputEventFilter::pinchGestureCancelled, &event);
}

void PointerInputRedirection::processHoldGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    PointerHoldGestureBeginEvent event{
        .fingerCount = fingerCount,
        .time = time,
    };

    input()->processSpies(&InputEventSpy::holdGestureBegin, &event);
    input()->processFilters(&InputEventFilter::holdGestureBegin, &event);
}

void PointerInputRedirection::processHoldGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    PointerHoldGestureEndEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::holdGestureEnd, &event);
    input()->processFilters(&InputEventFilter::holdGestureEnd, &event);
}

void PointerInputRedirection::processHoldGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    PointerHoldGestureCancelEvent event{
        .time = time,
    };

    input()->processSpies(&InputEventSpy::holdGestureCancelled, &event);
    input()->processFilters(&InputEventFilter::holdGestureCancelled, &event);
}

void PointerInputRedirection::processFrame(KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }

    input()->processFilters(&InputEventFilter::pointerFrame);
}

bool PointerInputRedirection::areButtonsPressed() const
{
    for (auto state : m_buttons) {
        if (state == PointerButtonState::Pressed) {
            return true;
        }
    }
    return false;
}

bool PointerInputRedirection::focusUpdatesBlocked()
{
    if (waylandServer()->seat()->isDragPointer()) {
        // ignore during drag and drop
        return true;
    }
    if (waylandServer()->seat()->isTouchSequence()) {
        // ignore during touch operations
        return true;
    }
    if (input()->isSelectingWindow()) {
        return true;
    }
    if (areButtonsPressed()) {
        return true;
    }
    return false;
}

void PointerInputRedirection::cleanupDecoration(Decoration::DecoratedWindowImpl *old, Decoration::DecoratedWindowImpl *now)
{
    disconnect(m_decorationGeometryConnection);
    m_decorationGeometryConnection = QMetaObject::Connection();

    disconnect(m_decorationDestroyedConnection);
    m_decorationDestroyedConnection = QMetaObject::Connection();

    disconnect(m_decorationClosedConnection);
    m_decorationClosedConnection = QMetaObject::Connection();

    if (old) {
        // send leave event to old decoration
        QHoverEvent event(QEvent::HoverLeave, QPointF(-1, -1), QPointF());
        QCoreApplication::instance()->sendEvent(old->decoration(), &event);
    }
    if (!now) {
        // left decoration
        return;
    }

    auto pos = m_pos - now->window()->pos();
    QHoverEvent event(QEvent::HoverEnter, pos, QPointF(-1, -1));
    QCoreApplication::instance()->sendEvent(now->decoration(), &event);
    now->window()->processDecorationMove(pos, m_pos);

    m_decorationGeometryConnection = connect(decoration()->window(), &Window::frameGeometryChanged, this, [this]() {
        // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
        const auto oldDeco = decoration();
        update();
        if (oldDeco && oldDeco == decoration() && !decoration()->window()->isInteractiveMove() && !decoration()->window()->isInteractiveResize() && !areButtonsPressed()) {
            // position of window did not change, we need to send HoverMotion manually
            const QPointF p = m_pos - decoration()->window()->pos();
            QHoverEvent event(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
        }
    });

    auto resetDecoration = [this]() {
        setDecoration(nullptr); // explicitly reset decoration if focus updates are blocked
        update();
    };

    m_decorationClosedConnection = connect(decoration()->window(), &Window::closed, this, resetDecoration);
    m_decorationDestroyedConnection = connect(now, &QObject::destroyed, this, resetDecoration);
}

void PointerInputRedirection::focusUpdate(Window *focusOld, Window *focusNow)
{
    if (focusOld && focusOld->isClient()) {
        focusOld->pointerLeaveEvent();
        breakPointerConstraints(focusOld->surface());
        disconnectPointerConstraintsConnection();
    }
    disconnect(m_focusGeometryConnection);
    m_focusGeometryConnection = QMetaObject::Connection();

    if (focusNow && focusNow->isClient()) {
        focusNow->pointerEnterEvent(m_pos);
    }

    auto seat = waylandServer()->seat();
    if (!focusNow || !focusNow->surface()) {
        seat->notifyPointerLeave();
        return;
    }

    seat->notifyPointerEnter(focusNow->surface(), m_pos, focusNow->inputTransformation());

    m_focusGeometryConnection = connect(focusNow, &Window::inputTransformationChanged, this, [this]() {
        waylandServer()->seat()->setFocusedPointerSurfaceTransformation(focus()->inputTransformation());
    });

    m_constraintsConnection = connect(focusNow->surface(), &SurfaceInterface::pointerConstraintsChanged,
                                      this, &PointerInputRedirection::updatePointerConstraints);
    m_constraintsActivatedConnection = connect(workspace(), &Workspace::windowActivated,
                                               this, &PointerInputRedirection::updatePointerConstraints);
    updatePointerConstraints();
}

void PointerInputRedirection::breakPointerConstraints(SurfaceInterface *surface)
{
    // cancel pointer constraints
    if (surface) {
        auto c = surface->confinedPointer();
        if (c && c->isConfined()) {
            c->setConfined(false);
        }
        auto l = surface->lockedPointer();
        if (l && l->isLocked()) {
            l->setLocked(false);
        }
    }
    disconnectConfinedPointerRegionConnection();
    m_confined = false;
    m_locked = false;
}

void PointerInputRedirection::disconnectConfinedPointerRegionConnection()
{
    disconnect(m_confinedPointerRegionConnection);
    m_confinedPointerRegionConnection = QMetaObject::Connection();
}

void PointerInputRedirection::disconnectLockedPointerAboutToBeUnboundConnection()
{
    disconnect(m_lockedPointerAboutToBeUnboundConnection);
    m_lockedPointerAboutToBeUnboundConnection = QMetaObject::Connection();
}

void PointerInputRedirection::disconnectPointerConstraintsConnection()
{
    disconnect(m_constraintsConnection);
    m_constraintsConnection = QMetaObject::Connection();

    disconnect(m_constraintsActivatedConnection);
    m_constraintsActivatedConnection = QMetaObject::Connection();
}

void PointerInputRedirection::setEnableConstraints(bool set)
{
    if (m_enableConstraints == set) {
        return;
    }
    m_enableConstraints = set;
    updatePointerConstraints();
}

void PointerInputRedirection::updatePointerConstraints()
{
    if (!focus()) {
        return;
    }
    const auto s = focus()->surface();
    if (!s) {
        return;
    }
    if (s != waylandServer()->seat()->focusedPointerSurface()) {
        return;
    }
    if (!supportsWarping()) {
        return;
    }
    const bool canConstrain = m_enableConstraints && focus() == workspace()->activeWindow();
    const auto cf = s->confinedPointer();
    if (cf) {
        if (cf->isConfined()) {
            if (!canConstrain) {
                cf->setConfined(false);
                m_confined = false;
                disconnectConfinedPointerRegionConnection();
            }
            return;
        }
        if (canConstrain && cf->region().contains(flooredPoint(focus()->mapToLocal(m_pos)))) {
            cf->setConfined(true);
            m_confined = true;
            m_confinedPointerRegionConnection = connect(cf, &ConfinedPointerV1Interface::regionChanged, this, [this]() {
                if (!focus()) {
                    return;
                }
                const auto s = focus()->surface();
                if (!s) {
                    return;
                }
                const auto cf = s->confinedPointer();
                if (!cf->region().contains(flooredPoint(focus()->mapToLocal(m_pos)))) {
                    // pointer no longer in confined region, break the confinement
                    cf->setConfined(false);
                    m_confined = false;
                } else {
                    if (!cf->isConfined()) {
                        cf->setConfined(true);
                        m_confined = true;
                    }
                }
            });
            return;
        }
    } else {
        m_confined = false;
        disconnectConfinedPointerRegionConnection();
    }
    const auto lock = s->lockedPointer();
    if (lock) {
        if (lock->isLocked()) {
            if (!canConstrain) {
                const auto hint = lock->cursorPositionHint();
                lock->setLocked(false);
                m_locked = false;
                disconnectLockedPointerAboutToBeUnboundConnection();
                if (!(hint.x() < 0 || hint.y() < 0) && focus()) {
                    processWarp(focus()->mapFromLocal(hint), waylandServer()->seat()->timestamp());
                }
            }
            return;
        }
        if (canConstrain && lock->region().contains(flooredPoint(focus()->mapToLocal(m_pos)))) {
            lock->setLocked(true);
            m_locked = true;

            // The client might cancel pointer locking from its side by unbinding the LockedPointerInterface.
            // In this case the cached cursor position hint must be fetched before the resource goes away
            m_lockedPointerAboutToBeUnboundConnection = connect(lock, &LockedPointerV1Interface::aboutToBeDestroyed, this, [this, lock]() {
                const auto hint = lock->cursorPositionHint();
                if (hint.x() < 0 || hint.y() < 0 || !focus()) {
                    return;
                }
                auto globalHint = focus()->mapFromLocal(hint);

                // When the resource finally goes away, reposition the cursor according to the hint
                connect(lock, &LockedPointerV1Interface::destroyed, this, [this, globalHint]() {
                    processWarp(globalHint, waylandServer()->seat()->timestamp());
                });
            });
            // TODO: connect to region change - is it needed at all? If the pointer is locked it's always in the region
        }
    } else {
        m_locked = false;
        disconnectLockedPointerAboutToBeUnboundConnection();
    }
}

QPointF PointerInputRedirection::applyPointerConfinement(const QPointF &pos) const
{
    if (!focus()) {
        return pos;
    }
    auto s = focus()->surface();
    if (!s) {
        return pos;
    }
    auto cf = s->confinedPointer();
    if (!cf) {
        return pos;
    }
    if (!cf->isConfined()) {
        return pos;
    }

    const QPointF localPos = focus()->mapToLocal(pos);
    if (cf->region().contains(flooredPoint(localPos))) {
        return pos;
    }

    const QPointF currentPos = focus()->mapToLocal(m_pos);

    // allow either x or y to pass
    QPointF p(currentPos.x(), localPos.y());
    if (cf->region().contains(flooredPoint(p))) {
        return focus()->mapFromLocal(p);
    }

    p = QPointF(localPos.x(), currentPos.y());
    if (cf->region().contains(flooredPoint(p))) {
        return focus()->mapFromLocal(p);
    }

    return m_pos;
}

PointerInputRedirection::EdgeBarrierType PointerInputRedirection::edgeBarrierType(const QPointF &pos, const QRectF &lastOutputGeometry) const
{
    constexpr qreal cornerThreshold = 15;
    const auto moveResizeWindow = workspace()->moveResizeWindow();
    const bool onCorner = (pos - lastOutputGeometry.topLeft()).manhattanLength() <= cornerThreshold
        || (pos - lastOutputGeometry.bottomLeft()).manhattanLength() <= cornerThreshold
        || (pos - lastOutputGeometry.topRight()).manhattanLength() <= cornerThreshold
        || (pos - lastOutputGeometry.bottomRight()).manhattanLength() <= cornerThreshold;
    if (moveResizeWindow && moveResizeWindow->isInteractiveMove()) {
        return EdgeBarrierType::WindowMoveBarrier;
    } else if (moveResizeWindow && moveResizeWindow->isInteractiveResize()) {
        return EdgeBarrierType::WindowResizeBarrier;
    } else if (options->cornerBarrier() && onCorner) {
        return EdgeBarrierType::CornerBarrier;
    } else if (workspace()->screenEdges()->inApproachGeometry(pos.toPoint())) {
        return EdgeBarrierType::EdgeElementBarrier;
    } else {
        return EdgeBarrierType::NormalBarrier;
    }
}

qreal PointerInputRedirection::edgeBarrier(EdgeBarrierType type) const
{
    const auto barrierWidth = options->edgeBarrier();
    switch (type) {
    case EdgeBarrierType::WindowMoveBarrier:
    case EdgeBarrierType::WindowResizeBarrier:
        return 1.5 * barrierWidth;
    case EdgeBarrierType::EdgeElementBarrier:
        return 2 * barrierWidth;
    case EdgeBarrierType::CornerBarrier:
        return 2000;
    case EdgeBarrierType::NormalBarrier:
        return barrierWidth;
    default:
        Q_UNREACHABLE();
        return 0;
    }
}

QPointF PointerInputRedirection::applyEdgeBarrier(const QPointF &pos, const Output *currentOutput, std::chrono::microseconds time)
{
    // optimization to avoid looping over all outputs
    if (exclusiveContains(currentOutput->geometry(), m_pos)) {
        m_movementInEdgeBarrier = QPointF();
        return pos;
    }
    const Output *lastOutput = workspace()->outputAt(m_pos);
    QPointF newPos = confineToBoundingBox(pos, lastOutput->geometry());
    const auto type = edgeBarrierType(newPos, lastOutput->geometry());
    if (m_lastEdgeBarrierType != type) {
        m_movementInEdgeBarrier = QPointF();
    }
    m_lastEdgeBarrierType = type;
    const auto barrierWidth = edgeBarrier(type);
    const qreal returnSpeed = barrierWidth / 10.0 /* px/s */ / 1000'000.0; // px/us
    std::chrono::microseconds timeDiff(time - m_lastMoveTime);
    qreal returnDistance = returnSpeed * timeDiff.count();

    const auto euclideanLength = [](const QPointF &point) {
        return std::sqrt(point.x() * point.x() + point.y() * point.y());
    };
    const auto shorten = [euclideanLength](const QPointF &point, const qreal distance) {
        const qreal length = euclideanLength(point);
        if (length <= distance) {
            return QPointF();
        }
        return point * (1 - distance / length);
    };

    m_movementInEdgeBarrier += (pos - newPos);
    m_movementInEdgeBarrier = shorten(m_movementInEdgeBarrier, returnDistance);

    if (euclideanLength(m_movementInEdgeBarrier) > barrierWidth) {
        newPos += shorten(m_movementInEdgeBarrier, barrierWidth);
        m_movementInEdgeBarrier = QPointF();
    }
    return newPos;
}

void PointerInputRedirection::updatePosition(const QPointF &pos, std::chrono::microseconds time)
{
    m_lastMoveTime = time;
    if (m_locked) {
        // locked pointer should not move
        return;
    }
    // verify that at least one screen contains the pointer position
    const Output *currentOutput = workspace()->outputAt(pos);
    QPointF p = confineToBoundingBox(pos, currentOutput->geometry());
    p = applyEdgeBarrier(p, currentOutput, time);
    p = applyPointerConfinement(p);
    if (p == m_pos) {
        // didn't change due to confinement
        return;
    }
    // verify screen confinement
    if (!screenContainsPos(p)) {
        return;
    }

    m_pos = p;

    workspace()->setActiveOutput(m_pos);
    m_cursor->updateCursorOutputs(m_pos);
    Cursors::self()->mouse()->setPos(m_pos);

    Q_EMIT input()->globalPointerChanged(m_pos);
}

void PointerInputRedirection::updateButton(uint32_t button, PointerButtonState state)
{
    m_buttons[button] = state;

    // update Qt buttons
    m_qtButtons = Qt::NoButton;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        if (it.value() == PointerButtonState::Released) {
            continue;
        }
        m_qtButtons |= buttonToQtMouseButton(it.key());
    }

    Q_EMIT input()->pointerButtonStateChanged(button, state);
}

void PointerInputRedirection::warp(const QPointF &pos)
{
    if (supportsWarping()) {
        processWarp(pos, waylandServer()->seat()->timestamp());
    }
}

bool PointerInputRedirection::supportsWarping() const
{
    return inited();
}

void PointerInputRedirection::updateAfterScreenChange()
{
    if (!inited()) {
        return;
    }

    Output *output = nullptr;
    if (m_lastOutputWasPlaceholder) {
        // previously we've positioned our pointer on a placeholder screen, try
        // to get us onto the real "primary" screen instead.
        output = workspace()->outputOrder().at(0);
    } else {
        if (screenContainsPos(m_pos)) {
            // pointer still on a screen
            return;
        }

        // pointer no longer on a screen, reposition to closes screen
        output = workspace()->outputAt(m_pos);
    }

    m_lastOutputWasPlaceholder = output->isPlaceholder();
    // TODO: better way to get timestamps
    processMotionAbsolute(output->geometry().center(), waylandServer()->seat()->timestamp());
}

QPointF PointerInputRedirection::position() const
{
    return m_pos;
}

void PointerInputRedirection::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    if (!inited()) {
        return;
    }
    // current pointer focus window should get a leave event
    update();
    m_cursor->setEffectsOverrideCursor(shape);
}

void PointerInputRedirection::removeEffectsOverrideCursor()
{
    if (!inited()) {
        return;
    }
    // cursor position might have changed while there was an effect in place
    update();
    m_cursor->removeEffectsOverrideCursor();
}

void PointerInputRedirection::setWindowSelectionCursor(const QByteArray &shape)
{
    if (!inited()) {
        return;
    }
    // send leave to current pointer focus window
    updateToReset();
    m_cursor->setWindowSelectionCursor(shape);
}

void PointerInputRedirection::removeWindowSelectionCursor()
{
    if (!inited()) {
        return;
    }
    update();
    m_cursor->removeWindowSelectionCursor();
}

CursorImage::CursorImage(PointerInputRedirection *parent)
    : QObject(parent)
    , m_pointer(parent)
{
    m_effectsCursor = std::make_unique<ShapeCursorSource>();
    m_fallbackCursor = std::make_unique<ShapeCursorSource>();
    m_moveResizeCursor = std::make_unique<ShapeCursorSource>();
    m_windowSelectionCursor = std::make_unique<ShapeCursorSource>();
    m_decoration.cursor = std::make_unique<ShapeCursorSource>();
    m_serverCursor.surface = std::make_unique<SurfaceCursorSource>();
    m_serverCursor.shape = std::make_unique<ShapeCursorSource>();
    m_dragCursor = std::make_unique<ShapeCursorSource>();

#if KWIN_BUILD_SCREENLOCKER
    if (kwinApp()->supportsLockScreen()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &CursorImage::reevaluteSource);
    }
#endif
    connect(m_pointer, &PointerInputRedirection::decorationChanged, this, &CursorImage::updateDecoration);
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this](Window *window) {
        connect(window, &Window::moveResizedChanged, this, &CursorImage::updateMoveResize);
        connect(window, &Window::moveResizeCursorChanged, this, &CursorImage::updateMoveResize);
    };
    const auto clients = workspace()->windows();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::windowAdded, this, setupMoveResizeConnection);

    m_fallbackCursor->setShape(Qt::ArrowCursor);

    m_effectsCursor->setTheme(m_waylandImage.theme());
    m_fallbackCursor->setTheme(m_waylandImage.theme());
    m_moveResizeCursor->setTheme(m_waylandImage.theme());
    m_windowSelectionCursor->setTheme(m_waylandImage.theme());
    m_decoration.cursor->setTheme(m_waylandImage.theme());
    m_serverCursor.shape->setTheme(m_waylandImage.theme());
    m_dragCursor->setTheme(m_waylandImage.theme());

    connect(&m_waylandImage, &WaylandCursorImage::themeChanged, this, [this] {
        m_effectsCursor->setTheme(m_waylandImage.theme());
        m_fallbackCursor->setTheme(m_waylandImage.theme());
        m_moveResizeCursor->setTheme(m_waylandImage.theme());
        m_windowSelectionCursor->setTheme(m_waylandImage.theme());
        m_decoration.cursor->setTheme(m_waylandImage.theme());
        m_serverCursor.shape->setTheme(m_waylandImage.theme());
        m_dragCursor->setTheme(m_waylandImage.theme());
    });

    connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, [this]() {
        m_dragCursor->setShape(Qt::ForbiddenCursor);
        connect(waylandServer()->seat()->dragSource(), &AbstractDataSource::dndActionChanged, this, &CursorImage::updateDragCursor);
        connect(waylandServer()->seat()->dragSource(), &AbstractDataSource::acceptedChanged, this, &CursorImage::updateDragCursor);
        reevaluteSource();
    });

    PointerInterface *pointer = waylandServer()->seat()->pointer();

    connect(pointer, &PointerInterface::focusedSurfaceChanged,
            this, &CursorImage::handleFocusedSurfaceChanged);

    reevaluteSource();
}

CursorImage::~CursorImage() = default;

void CursorImage::updateCursorOutputs(const QPointF &pos)
{
    if (m_currentSource == m_serverCursor.surface.get()) {
        auto cursorSurface = m_serverCursor.surface->surface();
        if (cursorSurface) {
            const QRectF cursorGeometry(pos - m_currentSource->hotspot(), m_currentSource->size());
            cursorSurface->setOutputs(waylandServer()->display()->outputsIntersecting(cursorGeometry.toAlignedRect()),
                                      waylandServer()->display()->largestIntersectingOutput(cursorGeometry.toAlignedRect()));
        }
    }
}

void CursorImage::handleFocusedSurfaceChanged()
{
    PointerInterface *pointer = waylandServer()->seat()->pointer();
    disconnect(m_serverCursor.connection);

    if (pointer->focusedSurface()) {
        m_serverCursor.connection = connect(pointer, &PointerInterface::cursorChanged,
                                            this, &CursorImage::updateServerCursor);
    } else {
        m_serverCursor.connection = QMetaObject::Connection();
        reevaluteSource();
    }
}

void CursorImage::updateDecoration()
{
    disconnect(m_decoration.connection);
    auto deco = m_pointer->decoration();
    Window *window = deco ? deco->window() : nullptr;
    if (window) {
        m_decoration.connection = connect(window, &Window::moveResizeCursorChanged, this, &CursorImage::updateDecorationCursor);
    } else {
        m_decoration.connection = QMetaObject::Connection();
    }
    updateDecorationCursor();
}

void CursorImage::updateDecorationCursor()
{
    auto deco = m_pointer->decoration();
    if (Window *window = deco ? deco->window() : nullptr) {
        m_decoration.cursor->setShape(window->cursor().name());
    }
    reevaluteSource();
}

void CursorImage::updateMoveResize()
{
    if (Window *window = workspace()->moveResizeWindow()) {
        m_moveResizeCursor->setShape(window->cursor().name());
    }
    reevaluteSource();
}

void CursorImage::updateDragCursor()
{
    AbstractDataSource *dragSource = waylandServer()->seat()->dragSource();
    if (dragSource && dragSource->isAccepted()) {
        switch (dragSource->selectedDndAction()) {
        case DataDeviceManagerInterface::DnDAction::None:
            m_dragCursor->setShape(Qt::ClosedHandCursor);
            break;
        case DataDeviceManagerInterface::DnDAction::Copy:
            m_dragCursor->setShape(Qt::DragCopyCursor);
            break;
        case DataDeviceManagerInterface::DnDAction::Move:
            m_dragCursor->setShape(Qt::DragMoveCursor);
            break;
        case DataDeviceManagerInterface::DnDAction::Ask:
            // Cursor themes don't have anything better in the themes yet
            // a dnd-drag-ask is proposed
            m_dragCursor->setShape(Qt::ClosedHandCursor);
            break;
        }
    } else {
        m_dragCursor->setShape(Qt::ForbiddenCursor);
    }
    reevaluteSource();
}

void CursorImage::updateServerCursor(const PointerCursor &cursor)
{
    if (auto surfaceCursor = std::get_if<PointerSurfaceCursor *>(&cursor)) {
        m_serverCursor.surface->update((*surfaceCursor)->surface(), (*surfaceCursor)->hotspot());
        m_serverCursor.cursor = m_serverCursor.surface.get();
    } else if (auto shapeCursor = std::get_if<QByteArray>(&cursor)) {
        m_serverCursor.shape->setShape(*shapeCursor);
        m_serverCursor.cursor = m_serverCursor.shape.get();
    }
    reevaluteSource();
}

void CursorImage::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    m_effectsCursor->setShape(shape);
    reevaluteSource();
}

void CursorImage::removeEffectsOverrideCursor()
{
    reevaluteSource();
}

void CursorImage::setWindowSelectionCursor(const QByteArray &shape)
{
    if (shape.isEmpty()) {
        m_windowSelectionCursor->setShape(Qt::CrossCursor);
    } else {
        m_windowSelectionCursor->setShape(shape);
    }
    reevaluteSource();
}

void CursorImage::removeWindowSelectionCursor()
{
    reevaluteSource();
}

WaylandCursorImage::WaylandCursorImage(QObject *parent)
    : QObject(parent)
{
    Cursor *pointerCursor = Cursors::self()->mouse();
    updateCursorTheme();

    connect(pointerCursor, &Cursor::themeChanged, this, &WaylandCursorImage::updateCursorTheme);
    connect(workspace(), &Workspace::outputsChanged, this, &WaylandCursorImage::updateCursorTheme);
}

CursorTheme WaylandCursorImage::theme() const
{
    return m_cursorTheme;
}

void WaylandCursorImage::updateCursorTheme()
{
    const Cursor *pointerCursor = Cursors::self()->mouse();
    qreal targetDevicePixelRatio = 1;

    const auto outputs = workspace()->outputs();
    for (const Output *output : outputs) {
        if (output->scale() > targetDevicePixelRatio) {
            targetDevicePixelRatio = output->scale();
        }
    }

    m_cursorTheme = CursorTheme(pointerCursor->themeName(), pointerCursor->themeSize(), targetDevicePixelRatio);
    if (m_cursorTheme.isEmpty()) {
        qCWarning(KWIN_CORE) << "Failed to load cursor theme" << pointerCursor->themeName();
        m_cursorTheme = CursorTheme(Cursor::defaultThemeName(), Cursor::defaultThemeSize(), targetDevicePixelRatio);

        if (m_cursorTheme.isEmpty()) {
            qCWarning(KWIN_CORE) << "Failed to load cursor theme" << Cursor::defaultThemeName();
            m_cursorTheme = CursorTheme(Cursor::fallbackThemeName(), Cursor::defaultThemeSize(), targetDevicePixelRatio);
        }
    }

    if (m_cursorTheme.isEmpty()) {
        qCWarning(KWIN_CORE) << "Unable to load any cursor theme";
    }

    Q_EMIT themeChanged();
}

void CursorImage::reevaluteSource()
{
    if (waylandServer()->isScreenLocked()) {
        setSource(m_serverCursor.cursor);
        return;
    }
    if (waylandServer()->seat()->isDrag()) {
        setSource(m_dragCursor.get());
        return;
    }
    if (input()->isSelectingWindow()) {
        setSource(m_windowSelectionCursor.get());
        return;
    }
    if (effects && effects->isMouseInterception()) {
        setSource(m_effectsCursor.get());
        return;
    }
    if (workspace() && workspace()->moveResizeWindow()) {
        setSource(m_moveResizeCursor.get());
        return;
    }
    if (m_pointer->decoration()) {
        setSource(m_decoration.cursor.get());
        return;
    }
    const PointerInterface *pointer = waylandServer()->seat()->pointer();
    if (pointer && pointer->focusedSurface()) {
        setSource(m_serverCursor.cursor);
        return;
    }
    setSource(m_fallbackCursor.get());
}

CursorSource *CursorImage::source() const
{
    return m_currentSource;
}

void CursorImage::setSource(CursorSource *source)
{
    if (m_currentSource == source) {
        return;
    }
    m_currentSource = source;
    Q_EMIT changed();
}

CursorTheme CursorImage::theme() const
{
    return m_waylandImage.theme();
}
}

#include "moc_pointer_input.cpp"
