/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "pointer_input.h"

#include <config-kwin.h>

#include "core/output.h"
#include "cursorsource.h"
#include "decorations/decoratedclient.h"
#include "effects.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "mousebuttons.h"
#include "osd.h"
#include "wayland/display.h"
#include "wayland/pointer_interface.h"
#include "wayland/pointerconstraints_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
// KDecoration
#include <KDecoration2/Decoration>
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

void PointerInputRedirection::init()
{
    Q_ASSERT(!inited());
    waylandServer()->seat()->setHasPointer(input()->hasPointer());
    connect(input(), &InputRedirection::hasPointerChanged,
            waylandServer()->seat(), &KWaylandServer::SeatInterface::setHasPointer);

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

    connect(Cursors::self()->mouse(), &Cursor::rendered, m_cursor, &CursorImage::markAsRendered);
    connect(m_cursor, &CursorImage::changed, Cursors::self()->mouse(), [this] {
        Cursors::self()->mouse()->setSource(m_cursor->source());
        updateCursorOutputs();
    });
    Q_EMIT m_cursor->changed();

    connect(workspace(), &Workspace::outputsChanged, this, &PointerInputRedirection::updateAfterScreenChange);
#if KWIN_BUILD_SCREENLOCKER
    if (waylandServer()->hasScreenLockerIntegration()) {
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
    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::dragEnded, this, [this]() {
        // need to force a focused pointer change
        setFocus(nullptr);
        update();
    });
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this](Window *window) {
        connect(window, &Window::clientStartUserMovedResized, this, &PointerInputRedirection::updateOnStartMoveResize);
        connect(window, &Window::clientFinishUserMovedResized, this, &PointerInputRedirection::update);
    };
    const auto clients = workspace()->allClientList();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::windowAdded, this, setupMoveResizeConnection);

    // warp the cursor to center of screen containing the workspace center
    if (const Output *output = workspace()->outputAt(workspace()->geometry().center())) {
        warp(output->geometry().center());
    }
    updateAfterScreenChange();
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
                m_pointer->processMotionInternal(pos.pos, pos.delta, pos.deltaNonAccelerated, pos.time, nullptr);
            }
        }
    }

    static bool isPositionBlocked()
    {
        return s_counter > 0;
    }

    static void schedulePosition(const QPointF &pos, const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time)
    {
        s_scheduledPositions.append({pos, delta, deltaNonAccelerated, time});
    }

private:
    static int s_counter;
    struct ScheduledPosition
    {
        QPointF pos;
        QPointF delta;
        QPointF deltaNonAccelerated;
        std::chrono::microseconds time;
    };
    static QVector<ScheduledPosition> s_scheduledPositions;

    PointerInputRedirection *m_pointer;
};

int PositionUpdateBlocker::s_counter = 0;
QVector<PositionUpdateBlocker::ScheduledPosition> PositionUpdateBlocker::s_scheduledPositions;

void PointerInputRedirection::processMotionAbsolute(const QPointF &pos, std::chrono::microseconds time, InputDevice *device)
{
    processMotionInternal(pos, QPointF(), QPointF(), time, device);
}

void PointerInputRedirection::processMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device)
{
    processMotionInternal(m_pos + delta, delta, deltaNonAccelerated, time, device);
}

void PointerInputRedirection::processMotionInternal(const QPointF &pos, const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    if (PositionUpdateBlocker::isPositionBlocked()) {
        PositionUpdateBlocker::schedulePosition(pos, delta, deltaNonAccelerated, time);
        return;
    }

    PositionUpdateBlocker blocker(this);
    updatePosition(pos);
    MouseEvent event(QEvent::MouseMove, m_pos, Qt::NoButton, m_qtButtons,
                     input()->keyboardModifiers(), time,
                     delta, deltaNonAccelerated, device);
    event.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());

    update();
    input()->processSpies(std::bind(&InputEventSpy::pointerEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::pointerEvent, std::placeholders::_1, &event, 0));
}

void PointerInputRedirection::processButton(uint32_t button, InputRedirection::PointerButtonState state, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    QEvent::Type type;
    switch (state) {
    case InputRedirection::PointerButtonReleased:
        type = QEvent::MouseButtonRelease;
        break;
    case InputRedirection::PointerButtonPressed:
        type = QEvent::MouseButtonPress;
        update();
        break;
    default:
        Q_UNREACHABLE();
        return;
    }

    updateButton(button, state);

    MouseEvent event(type, m_pos, buttonToQtMouseButton(button), m_qtButtons,
                     input()->keyboardModifiers(), time, QPointF(), QPointF(), device);
    event.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());
    event.setNativeButton(button);

    input()->processSpies(std::bind(&InputEventSpy::pointerEvent, std::placeholders::_1, &event));

    if (!inited()) {
        return;
    }

    input()->processFilters(std::bind(&InputEventFilter::pointerEvent, std::placeholders::_1, &event, button));

    if (state == InputRedirection::PointerButtonReleased) {
        update();
    }
}

void PointerInputRedirection::processAxis(InputRedirection::PointerAxis axis, qreal delta, qint32 deltaV120,
                                          InputRedirection::PointerAxisSource source, std::chrono::microseconds time, InputDevice *device)
{
    input()->setLastInputHandler(this);
    update();

    Q_EMIT input()->pointerAxisChanged(axis, delta);

    WheelEvent wheelEvent(m_pos, delta, deltaV120,
                          (axis == InputRedirection::PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                          m_qtButtons, input()->keyboardModifiers(), source, time, device);
    wheelEvent.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());

    input()->processSpies(std::bind(&InputEventSpy::wheelEvent, std::placeholders::_1, &wheelEvent));

    if (!inited()) {
        return;
    }
    input()->processFilters(std::bind(&InputEventFilter::wheelEvent, std::placeholders::_1, &wheelEvent));
}

void PointerInputRedirection::processSwipeGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureBegin, std::placeholders::_1, fingerCount, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureBegin, std::placeholders::_1, fingerCount, time));
}

void PointerInputRedirection::processSwipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureUpdate, std::placeholders::_1, delta, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureUpdate, std::placeholders::_1, delta, time));
}

void PointerInputRedirection::processSwipeGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureEnd, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureEnd, std::placeholders::_1, time));
}

void PointerInputRedirection::processSwipeGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureCancelled, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureCancelled, std::placeholders::_1, time));
}

void PointerInputRedirection::processPinchGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureBegin, std::placeholders::_1, fingerCount, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureBegin, std::placeholders::_1, fingerCount, time));
}

void PointerInputRedirection::processPinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureUpdate, std::placeholders::_1, scale, angleDelta, delta, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureUpdate, std::placeholders::_1, scale, angleDelta, delta, time));
}

void PointerInputRedirection::processPinchGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureEnd, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureEnd, std::placeholders::_1, time));
}

void PointerInputRedirection::processPinchGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    input()->setLastInputHandler(this);
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureCancelled, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureCancelled, std::placeholders::_1, time));
}

void PointerInputRedirection::processHoldGestureBegin(int fingerCount, std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::holdGestureBegin, std::placeholders::_1, fingerCount, time));
    input()->processFilters(std::bind(&InputEventFilter::holdGestureBegin, std::placeholders::_1, fingerCount, time));
}

void PointerInputRedirection::processHoldGestureEnd(std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::holdGestureEnd, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::holdGestureEnd, std::placeholders::_1, time));
}

void PointerInputRedirection::processHoldGestureCancelled(std::chrono::microseconds time, KWin::InputDevice *device)
{
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::holdGestureCancelled, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::holdGestureCancelled, std::placeholders::_1, time));
}

bool PointerInputRedirection::areButtonsPressed() const
{
    for (auto state : m_buttons) {
        if (state == InputRedirection::PointerButtonPressed) {
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

void PointerInputRedirection::cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now)
{
    disconnect(m_decorationGeometryConnection);
    m_decorationGeometryConnection = QMetaObject::Connection();

    disconnect(m_decorationDestroyedConnection);
    m_decorationDestroyedConnection = QMetaObject::Connection();

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

    m_decorationGeometryConnection = connect(
        decoration()->window(), &Window::frameGeometryChanged, this, [this]() {
            // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
            const auto oldDeco = decoration();
            update();
            if (oldDeco && oldDeco == decoration() && !decoration()->window()->isInteractiveMove() && !decoration()->window()->isInteractiveResize() && !areButtonsPressed()) {
                // position of window did not change, we need to send HoverMotion manually
                const QPointF p = m_pos - decoration()->window()->pos();
                QHoverEvent event(QEvent::HoverMove, p, p);
                QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
            }
        },
        Qt::QueuedConnection);

    // if our decoration gets destroyed whilst it has focus, we pass focus on to the same window
    m_decorationDestroyedConnection = connect(now, &QObject::destroyed, this, &PointerInputRedirection::update, Qt::QueuedConnection);
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
        // TODO: why no assert possible?
        if (!focus()) {
            return;
        }
        // TODO: can we check on the window instead?
        if (workspace()->moveResizeWindow()) {
            // don't update while moving
            return;
        }
        auto seat = waylandServer()->seat();
        if (focus()->surface() != seat->focusedPointerSurface()) {
            return;
        }
        seat->setFocusedPointerSurfaceTransformation(focus()->inputTransformation());
    });

    m_constraintsConnection = connect(focusNow->surface(), &KWaylandServer::SurfaceInterface::pointerConstraintsChanged,
                                      this, &PointerInputRedirection::updatePointerConstraints);
    m_constraintsActivatedConnection = connect(workspace(), &Workspace::windowActivated,
                                               this, &PointerInputRedirection::updatePointerConstraints);
    updatePointerConstraints();
}

void PointerInputRedirection::breakPointerConstraints(KWaylandServer::SurfaceInterface *surface)
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

template<typename T>
static QRegion getConstraintRegion(Window *window, T *constraint)
{
    const QRegion windowShape = window->inputShape();
    const QRegion intersected = constraint->region().isEmpty() ? windowShape : windowShape.intersected(constraint->region());
    return intersected.translated(window->mapFromLocal(QPointF(0, 0)).toPoint());
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
        const QRegion r = getConstraintRegion(focus(), cf);
        if (canConstrain && r.contains(m_pos.toPoint())) {
            cf->setConfined(true);
            m_confined = true;
            m_confinedPointerRegionConnection = connect(cf, &KWaylandServer::ConfinedPointerV1Interface::regionChanged, this, [this]() {
                if (!focus()) {
                    return;
                }
                const auto s = focus()->surface();
                if (!s) {
                    return;
                }
                const auto cf = s->confinedPointer();
                if (!getConstraintRegion(focus(), cf).contains(m_pos.toPoint())) {
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
                    processMotionAbsolute(focus()->mapFromLocal(hint), waylandServer()->seat()->timestamp());
                }
            }
            return;
        }
        const QRegion r = getConstraintRegion(focus(), lock);
        if (canConstrain && r.contains(m_pos.toPoint())) {
            lock->setLocked(true);
            m_locked = true;

            // The client might cancel pointer locking from its side by unbinding the LockedPointerInterface.
            // In this case the cached cursor position hint must be fetched before the resource goes away
            m_lockedPointerAboutToBeUnboundConnection = connect(lock, &KWaylandServer::LockedPointerV1Interface::aboutToBeDestroyed, this, [this, lock]() {
                const auto hint = lock->cursorPositionHint();
                if (hint.x() < 0 || hint.y() < 0 || !focus()) {
                    return;
                }
                auto globalHint = focus()->mapFromLocal(hint);

                // When the resource finally goes away, reposition the cursor according to the hint
                connect(lock, &KWaylandServer::LockedPointerV1Interface::destroyed, this, [this, globalHint]() {
                    processMotionAbsolute(globalHint, waylandServer()->seat()->timestamp());
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

    const QRegion confinementRegion = getConstraintRegion(focus(), cf);
    if (confinementRegion.contains(flooredPoint(pos))) {
        return pos;
    }
    QPointF p = pos;
    // allow either x or y to pass
    p = QPointF(m_pos.x(), pos.y());

    if (confinementRegion.contains(flooredPoint(p))) {
        return p;
    }
    p = QPointF(pos.x(), m_pos.y());
    if (confinementRegion.contains(flooredPoint(p))) {
        return p;
    }

    return m_pos;
}

void PointerInputRedirection::updatePosition(const QPointF &pos)
{
    if (m_locked) {
        // locked pointer should not move
        return;
    }
    // verify that at least one screen contains the pointer position
    const Output *currentOutput = workspace()->outputAt(pos);
    QPointF p = confineToBoundingBox(pos, currentOutput->geometry());
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

    workspace()->setActiveCursorOutput(m_pos);
    updateCursorOutputs();

    Q_EMIT input()->globalPointerChanged(m_pos);
}

void PointerInputRedirection::updateCursorOutputs()
{
    KWaylandServer::PointerInterface *pointer = waylandServer()->seat()->pointer();
    if (!pointer) {
        return;
    }

    KWaylandServer::Cursor *cursor = pointer->cursor();
    if (!cursor) {
        return;
    }

    KWaylandServer::SurfaceInterface *surface = cursor->surface();
    if (!surface) {
        return;
    }

    const QRectF cursorGeometry(m_pos - m_cursor->source()->hotspot(), surface->size());
    surface->setOutputs(waylandServer()->display()->outputsIntersecting(cursorGeometry.toAlignedRect()));
}

void PointerInputRedirection::updateButton(uint32_t button, InputRedirection::PointerButtonState state)
{
    m_buttons[button] = state;

    // update Qt buttons
    m_qtButtons = Qt::NoButton;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        if (it.value() == InputRedirection::PointerButtonReleased) {
            continue;
        }
        m_qtButtons |= buttonToQtMouseButton(it.key());
    }

    Q_EMIT input()->pointerButtonStateChanged(button, state);
}

void PointerInputRedirection::warp(const QPointF &pos)
{
    if (supportsWarping()) {
        processMotionAbsolute(pos, waylandServer()->seat()->timestamp());
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
    if (screenContainsPos(m_pos)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    const Output *output = workspace()->outputAt(m_pos);
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
    m_serverCursor.cursor = std::make_unique<SurfaceCursorSource>();

#if KWIN_BUILD_SCREENLOCKER
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &CursorImage::reevaluteSource);
    }
#endif
    connect(m_pointer, &PointerInputRedirection::decorationChanged, this, &CursorImage::updateDecoration);
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this](Window *window) {
        connect(window, &Window::moveResizedChanged, this, &CursorImage::updateMoveResize);
        connect(window, &Window::moveResizeCursorChanged, this, &CursorImage::updateMoveResize);
    };
    const auto clients = workspace()->allClientList();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::windowAdded, this, setupMoveResizeConnection);

    m_fallbackCursor->setShape(Qt::ArrowCursor);

    m_effectsCursor->setTheme(m_waylandImage.theme());
    m_fallbackCursor->setTheme(m_waylandImage.theme());
    m_moveResizeCursor->setTheme(m_waylandImage.theme());
    m_windowSelectionCursor->setTheme(m_waylandImage.theme());
    m_decoration.cursor->setTheme(m_waylandImage.theme());

    connect(&m_waylandImage, &WaylandCursorImage::themeChanged, this, [this] {
        m_effectsCursor->setTheme(m_waylandImage.theme());
        m_fallbackCursor->setTheme(m_waylandImage.theme());
        m_moveResizeCursor->setTheme(m_waylandImage.theme());
        m_windowSelectionCursor->setTheme(m_waylandImage.theme());
        m_decoration.cursor->setTheme(m_waylandImage.theme());
    });

    KWaylandServer::PointerInterface *pointer = waylandServer()->seat()->pointer();

    connect(pointer, &KWaylandServer::PointerInterface::focusedSurfaceChanged,
            this, &CursorImage::handleFocusedSurfaceChanged);

    reevaluteSource();
}

CursorImage::~CursorImage() = default;

void CursorImage::markAsRendered(std::chrono::milliseconds timestamp)
{
    if (m_currentSource != m_serverCursor.cursor.get()) {
        return;
    }
    auto p = waylandServer()->seat()->pointer();
    if (!p) {
        return;
    }
    auto c = p->cursor();
    if (!c) {
        return;
    }
    auto cursorSurface = c->surface();
    if (!cursorSurface) {
        return;
    }
    cursorSurface->frameRendered(timestamp.count());
}

void CursorImage::handleFocusedSurfaceChanged()
{
    KWaylandServer::PointerInterface *pointer = waylandServer()->seat()->pointer();
    disconnect(m_serverCursor.connection);

    if (pointer->focusedSurface()) {
        m_serverCursor.connection = connect(pointer, &KWaylandServer::PointerInterface::cursorChanged,
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

void CursorImage::updateServerCursor()
{
    reevaluteSource();
    auto p = waylandServer()->seat()->pointer();
    if (!p) {
        return;
    }
    auto c = p->cursor();
    if (c) {
        m_serverCursor.cursor->update(c->surface(), c->hotspot());
    }
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

KXcursorTheme WaylandCursorImage::theme() const
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

    m_cursorTheme = KXcursorTheme(pointerCursor->themeName(), pointerCursor->themeSize(), targetDevicePixelRatio);
    if (m_cursorTheme.isEmpty()) {
        m_cursorTheme = KXcursorTheme(Cursor::defaultThemeName(), Cursor::defaultThemeSize(), targetDevicePixelRatio);
    }

    Q_EMIT themeChanged();
}

void WaylandCursorImage::loadThemeCursor(const CursorShape &shape, ImageCursorSource *source)
{
    loadThemeCursor(shape.name(), source);
}

void WaylandCursorImage::loadThemeCursor(const QByteArray &name, ImageCursorSource *source)
{
    if (loadThemeCursor_helper(name, source)) {
        return;
    }

    const auto alternativeNames = Cursor::cursorAlternativeNames(name);
    for (const QByteArray &alternativeName : alternativeNames) {
        if (loadThemeCursor_helper(alternativeName, source)) {
            return;
        }
    }

    qCWarning(KWIN_CORE) << "Failed to load theme cursor for shape" << name;
}

bool WaylandCursorImage::loadThemeCursor_helper(const QByteArray &name, ImageCursorSource *source)
{
    const QVector<KXcursorSprite> sprites = m_cursorTheme.shape(name);
    if (sprites.isEmpty()) {
        return false;
    }
    source->update(sprites.first().data(), sprites.first().hotspot());
    return true;
}

void CursorImage::reevaluteSource()
{
    if (waylandServer()->isScreenLocked()) {
        setSource(m_serverCursor.cursor.get());
        return;
    }
    if (input()->isSelectingWindow()) {
        setSource(m_windowSelectionCursor.get());
        return;
    }
    if (effects && static_cast<EffectsHandlerImpl *>(effects)->isMouseInterception()) {
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
    const KWaylandServer::PointerInterface *pointer = waylandServer()->seat()->pointer();
    if (pointer && pointer->focusedSurface()) {
        setSource(m_serverCursor.cursor.get());
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

KXcursorTheme CursorImage::theme() const
{
    return m_waylandImage.theme();
}

InputRedirectionCursor::InputRedirectionCursor(QObject *parent)
    : Cursor(parent)
    , m_currentButtons(Qt::NoButton)
{
    Cursors::self()->setMouse(this);
    connect(input(), &InputRedirection::globalPointerChanged,
            this, &InputRedirectionCursor::slotPosChanged);
    connect(input(), &InputRedirection::pointerButtonStateChanged,
            this, &InputRedirectionCursor::slotPointerButtonChanged);
#ifndef KCMRULES
    connect(input(), &InputRedirection::keyboardModifiersChanged,
            this, &InputRedirectionCursor::slotModifiersChanged);
#endif
}

InputRedirectionCursor::~InputRedirectionCursor()
{
}

void InputRedirectionCursor::doSetPos()
{
    if (input()->supportsPointerWarping()) {
        input()->warpPointer(currentPos());
    }
    slotPosChanged(input()->globalPointer());
    Q_EMIT posChanged(currentPos());
}

void InputRedirectionCursor::slotPosChanged(const QPointF &pos)
{
    const QPoint oldPos = currentPos();
    updatePos(pos.toPoint());
    Q_EMIT mouseChanged(pos.toPoint(), oldPos, m_currentButtons, m_currentButtons,
                        input()->keyboardModifiers(), input()->keyboardModifiers());
}

void InputRedirectionCursor::slotModifiersChanged(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods)
{
    Q_EMIT mouseChanged(currentPos(), currentPos(), m_currentButtons, m_currentButtons, mods, oldMods);
}

void InputRedirectionCursor::slotPointerButtonChanged()
{
    const Qt::MouseButtons oldButtons = m_currentButtons;
    m_currentButtons = input()->qtButtonStates();
    const QPoint pos = currentPos();
    Q_EMIT mouseChanged(pos, pos, m_currentButtons, oldButtons, input()->keyboardModifiers(), input()->keyboardModifiers());
}

}
