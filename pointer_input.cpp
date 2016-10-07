/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "pointer_input.h"
#include "platform.h"
#include "effects.h"
#include "input_event.h"
#include "screens.h"
#include "shell_client.h"
#include "wayland_cursor_theme.h"
#include "wayland_server.h"
#include "workspace.h"
#include "decorations/decoratedclient.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/buffer.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// screenlocker
#include <KScreenLocker/KsldApp>

#include <QHoverEvent>
#include <QWindow>
// Wayland
#include <wayland-cursor.h>

#include <linux/input.h>

namespace KWin
{

static Qt::MouseButton buttonToQtMouseButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return Qt::LeftButton;
    case BTN_MIDDLE:
        return Qt::MiddleButton;
    case BTN_RIGHT:
        return Qt::RightButton;
    case BTN_SIDE:
        // in QtWayland mapped like that
        return Qt::ExtraButton1;
    case BTN_EXTRA:
        // in QtWayland mapped like that
        return Qt::ExtraButton2;
    case BTN_BACK:
        return Qt::BackButton;
    case BTN_FORWARD:
        return Qt::ForwardButton;
    case BTN_TASK:
        return Qt::TaskButton;
        // mapped like that in QtWayland
    case 0x118:
        return Qt::ExtraButton6;
    case 0x119:
        return Qt::ExtraButton7;
    case 0x11a:
        return Qt::ExtraButton8;
    case 0x11b:
        return Qt::ExtraButton9;
    case 0x11c:
        return Qt::ExtraButton10;
    case 0x11d:
        return Qt::ExtraButton11;
    case 0x11e:
        return Qt::ExtraButton12;
    case 0x11f:
        return Qt::ExtraButton13;
    }
    // all other values get mapped to ExtraButton24
    // this is actually incorrect but doesn't matter in our usage
    // KWin internally doesn't use these high extra buttons anyway
    // it's only needed for recognizing whether buttons are pressed
    // if multiple buttons are mapped to the value the evaluation whether
    // buttons are pressed is correct and that's all we care about.
    return Qt::ExtraButton24;
}

static bool screenContainsPos(const QPointF &pos)
{
    for (int i = 0; i < screens()->count(); ++i) {
        if (screens()->geometry(i).contains(pos.toPoint())) {
            return true;
        }
    }
    return false;
}

PointerInputRedirection::PointerInputRedirection(InputRedirection* parent)
    : InputDeviceHandler(parent)
    , m_cursor(nullptr)
    , m_supportsWarping(Application::usesLibinput())
{
}

PointerInputRedirection::~PointerInputRedirection() = default;

void PointerInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_cursor = new CursorImage(this);
    m_inited = true;
    connect(m_cursor, &CursorImage::changed, kwinApp()->platform(), &Platform::cursorChanged);
    emit m_cursor->changed();
    connect(workspace(), &Workspace::stackingOrderChanged, this, &PointerInputRedirection::update);
    connect(screens(), &Screens::changed, this, &PointerInputRedirection::updateAfterScreenChange);
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &PointerInputRedirection::update);
    }
    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragEnded, this,
        [this] {
            // need to force a focused pointer change
            waylandServer()->seat()->setFocusedPointerSurface(nullptr);
            m_window.clear();
            update();
        }
    );
    connect(this, &PointerInputRedirection::internalWindowChanged, this,
        [this] {
            disconnect(m_internalWindowConnection);
            m_internalWindowConnection = QMetaObject::Connection();
            if (m_internalWindow) {
                m_internalWindowConnection = connect(m_internalWindow.data(), &QWindow::visibleChanged, this,
                    [this] (bool visible) {
                        if (!visible) {
                            update();
                        }
                    }
                );
            }
        }
    );

    // warp the cursor to center of screen
    warp(screens()->geometry().center());
    updateAfterScreenChange();
}

void PointerInputRedirection::processMotion(const QPointF &pos, uint32_t time, LibInput::Device *device)
{
    processMotion(pos, QSizeF(), QSizeF(), time, 0, device);
}

void PointerInputRedirection::processMotion(const QPointF &pos, const QSizeF &delta, const QSizeF &deltaNonAccelerated, uint32_t time, quint64 timeUsec, LibInput::Device *device)
{
    if (!m_inited) {
        return;
    }
    updatePosition(pos);
    MouseEvent event(QEvent::MouseMove, m_pos, Qt::NoButton, m_qtButtons,
                     m_input->keyboardModifiers(), time,
                     delta, deltaNonAccelerated, timeUsec, device);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, 0)) {
            return;
        }
    }
}

void PointerInputRedirection::processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time, LibInput::Device *device)
{
    updateButton(button, state);

    if (!m_inited) {
        return;
    }

    QEvent::Type type;
    switch (state) {
    case InputRedirection::PointerButtonReleased:
        type = QEvent::MouseButtonRelease;
        break;
    case InputRedirection::PointerButtonPressed:
        type = QEvent::MouseButtonPress;
        break;
    default:
        Q_UNREACHABLE();
        return;
    }

    MouseEvent event(type, m_pos, buttonToQtMouseButton(button), m_qtButtons,
                     m_input->keyboardModifiers(), time, QSizeF(), QSizeF(), 0, device);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, button)) {
            return;
        }
    }
}

void PointerInputRedirection::processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time, LibInput::Device *device)
{
    if (delta == 0) {
        return;
    }

    emit m_input->pointerAxisChanged(axis, delta);

    if (!m_inited) {
        return;
    }

    WheelEvent wheelEvent(m_pos, delta,
                           (axis == InputRedirection::PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                           m_qtButtons, m_input->keyboardModifiers(), time, device);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->wheelEvent(&wheelEvent)) {
            return;
        }
    }
}

void PointerInputRedirection::processSwipeGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->swipeGestureBegin(fingerCount, time)) {
            return;
        }
    }
}

void PointerInputRedirection::processSwipeGestureUpdate(const QSizeF &delta, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->swipeGestureUpdate(delta, time)) {
            return;
        }
    }
}

void PointerInputRedirection::processSwipeGestureEnd(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->swipeGestureEnd(time)) {
            return;
        }
    }
}

void PointerInputRedirection::processSwipeGestureCancelled(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->swipeGestureCancelled(time)) {
            return;
        }
    }
}

void PointerInputRedirection::processPinchGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pinchGestureBegin(fingerCount, time)) {
            return;
        }
    }
}

void PointerInputRedirection::processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pinchGestureUpdate(scale, angleDelta, delta, time)) {
            return;
        }
    }
}

void PointerInputRedirection::processPinchGestureEnd(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pinchGestureEnd(time)) {
            return;
        }
    }
}

void PointerInputRedirection::processPinchGestureCancelled(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pinchGestureCancelled(time)) {
            return;
        }
    }
}

void PointerInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    if (waylandServer()->seat()->isDragPointer()) {
        // ignore during drag and drop
        return;
    }
    // TODO: handle pointer grab aka popups
    Toplevel *t = m_input->findToplevel(m_pos.toPoint());
    const auto oldDeco = m_decoration;
    updateInternalWindow(m_pos);
    if (!m_internalWindow) {
        updateDecoration(t, m_pos);
    } else {
        updateDecoration(waylandServer()->findClient(m_internalWindow), m_pos);
        if (m_decoration) {
            disconnect(m_internalWindowConnection);
            m_internalWindowConnection = QMetaObject::Connection();
            QEvent event(QEvent::Leave);
            QCoreApplication::sendEvent(m_internalWindow.data(), &event);
            m_internalWindow.clear();
        }
    }
    if (m_decoration || m_internalWindow) {
        t = nullptr;
    }
    if (m_decoration != oldDeco) {
        emit decorationChanged();
    }
    auto oldWindow = m_window;
    if (!oldWindow.isNull() && t == m_window.data()) {
        return;
    }
    auto seat = waylandServer()->seat();
    // disconnect old surface
    if (oldWindow) {
        if (AbstractClient *c = qobject_cast<AbstractClient*>(oldWindow.data())) {
            c->leaveEvent();
        }
        disconnect(m_windowGeometryConnection);
        m_windowGeometryConnection = QMetaObject::Connection();
    }
    if (AbstractClient *c = qobject_cast<AbstractClient*>(t)) {
        // only send enter if it wasn't on deco for the same client before
        if (m_decoration.isNull() || m_decoration->client() != c) {
            c->enterEvent(m_pos.toPoint());
            workspace()->updateFocusMousePosition(m_pos.toPoint());
        }
    }
    if (t && t->surface()) {
        m_window = QPointer<Toplevel>(t);
        // TODO: add convenient API to update global pos together with updating focused surface
        warpXcbOnSurfaceLeft(t->surface());
        seat->setFocusedPointerSurface(nullptr);
        seat->setPointerPos(m_pos.toPoint());
        seat->setFocusedPointerSurface(t->surface(), t->inputTransformation());
        m_windowGeometryConnection = connect(t, &Toplevel::geometryChanged, this,
            [this] {
                if (m_window.isNull()) {
                    return;
                }
                // TODO: can we check on the client instead?
                if (workspace()->getMovingClient()) {
                    // don't update while moving
                    return;
                }
                auto seat = waylandServer()->seat();
                if (m_window.data()->surface() != seat->focusedPointerSurface()) {
                    return;
                }
                seat->setFocusedPointerSurfaceTransformation(m_window.data()->inputTransformation());
            }
        );
    } else {
        m_window.clear();
        warpXcbOnSurfaceLeft(nullptr);
        seat->setFocusedPointerSurface(nullptr);
        t = nullptr;
    }
}

void PointerInputRedirection::warpXcbOnSurfaceLeft(KWayland::Server::SurfaceInterface *newSurface)
{
    auto xc = waylandServer()->xWaylandConnection();
    if (!xc) {
        // No XWayland, no point in warping the x cursor
        return;
    }
    if (!kwinApp()->x11Connection()) {
        return;
    }
    static bool s_hasXWayland119 = xcb_get_setup(kwinApp()->x11Connection())->release_number >= 11900000;
    if (s_hasXWayland119) {
        return;
    }
    if (newSurface && newSurface->client() == xc) {
        // new window is an X window
        return;
    }
    auto s = waylandServer()->seat()->focusedPointerSurface();
    if (!s || s->client() != xc) {
        // pointer was not on an X window
        return;
    }
    // warp pointer to 0/0 to trigger leave events on previously focused X window
    xcb_warp_pointer(connection(), XCB_WINDOW_NONE, rootWindow(), 0, 0, 0, 0, 0, 0),
    xcb_flush(connection());
}

void PointerInputRedirection::updatePosition(const QPointF &pos)
{
    // verify that at least one screen contains the pointer position
    QPointF p = pos;
    if (!screenContainsPos(p)) {
        // allow either x or y to pass
        p = QPointF(m_pos.x(), pos.y());
        if (!screenContainsPos(p)) {
            p = QPointF(pos.x(), m_pos.y());
            if (!screenContainsPos(p)) {
                return;
            }
        }
    }
    m_pos = p;
    emit m_input->globalPointerChanged(m_pos);
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

    emit m_input->pointerButtonStateChanged(button, state);
}

void PointerInputRedirection::warp(const QPointF &pos)
{
    if (supportsWarping()) {
        kwinApp()->platform()->warpPointer(pos);
        processMotion(pos, waylandServer()->seat()->timestamp());
    }
}

bool PointerInputRedirection::supportsWarping() const
{
    if (!m_inited) {
        return false;
    }
    if (m_supportsWarping) {
        return true;
    }
    if (kwinApp()->platform()->supportsPointerWarping()) {
        return true;
    }
    return false;
}

void PointerInputRedirection::updateAfterScreenChange()
{
    if (!m_inited) {
        return;
    }
    if (screenContainsPos(m_pos)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    const QPointF pos = screens()->geometry(screens()->number(m_pos.toPoint())).center();
    // TODO: better way to get timestamps
    processMotion(pos, waylandServer()->seat()->timestamp());
}

QImage PointerInputRedirection::cursorImage() const
{
    if (!m_inited) {
        return QImage();
    }
    return m_cursor->image();
}

QPoint PointerInputRedirection::cursorHotSpot() const
{
    if (!m_inited) {
        return QPoint();
    }
    return m_cursor->hotSpot();
}

void PointerInputRedirection::markCursorAsRendered()
{
    if (!m_inited) {
        return;
    }
    m_cursor->markAsRendered();
}

void PointerInputRedirection::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    if (!m_inited) {
        return;
    }
    // current pointer focus window should get a leave event
    update();
    m_cursor->setEffectsOverrideCursor(shape);
}

void PointerInputRedirection::removeEffectsOverrideCursor()
{
    if (!m_inited) {
        return;
    }
    // cursor position might have changed while there was an effect in place
    update();
    m_cursor->removeEffectsOverrideCursor();
}

CursorImage::CursorImage(PointerInputRedirection *parent)
    : QObject(parent)
    , m_pointer(parent)
{
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::focusedPointerChanged, this, &CursorImage::update);
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragStarted, this, &CursorImage::updateDrag);
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragEnded, this,
        [this] {
            disconnect(m_drag.connection);
            reevaluteSource();
        }
    );
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &CursorImage::reevaluteSource);
    }
    connect(m_pointer, &PointerInputRedirection::decorationChanged, this, &CursorImage::updateDecoration);
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this] (AbstractClient *c) {
        connect(c, &AbstractClient::moveResizedChanged, this, &CursorImage::updateMoveResize);
    };
    const auto clients = workspace()->allClientList();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::clientAdded, this, setupMoveResizeConnection);
    connect(waylandServer(), &WaylandServer::shellClientAdded, this, setupMoveResizeConnection);
    loadThemeCursor(Qt::ArrowCursor, &m_fallbackCursor);
    if (m_cursorTheme) {
        connect(m_cursorTheme, &WaylandCursorTheme::themeChanged, this,
            [this] {
                m_cursors.clear();
                loadThemeCursor(Qt::ArrowCursor, &m_fallbackCursor);
                updateDecorationCursor();
                updateMoveResize();
                // TODO: update effects
            }
        );
    }
    m_surfaceRenderedTimer.start();
}

CursorImage::~CursorImage() = default;

void CursorImage::markAsRendered()
{
    if (m_currentSource == CursorSource::DragAndDrop) {
        // always sending a frame rendered to the drag icon surface to not freeze QtWayland (see https://bugreports.qt.io/browse/QTBUG-51599 )
        if (auto ddi = waylandServer()->seat()->dragSource()) {
            if (auto s = ddi->icon()) {
                s->frameRendered(m_surfaceRenderedTimer.elapsed());
            }
        }
        auto p = waylandServer()->seat()->dragPointer();
        if (!p) {
            return;
        }
        auto c = p->cursor();
        if (!c) {
            return;
        }
        auto cursorSurface = c->surface();
        if (cursorSurface.isNull()) {
            return;
        }
        cursorSurface->frameRendered(m_surfaceRenderedTimer.elapsed());
        return;
    }
    if (m_currentSource != CursorSource::LockScreen && m_currentSource != CursorSource::PointerSurface) {
        return;
    }
    auto p = waylandServer()->seat()->focusedPointer();
    if (!p) {
        return;
    }
    auto c = p->cursor();
    if (!c) {
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        return;
    }
    cursorSurface->frameRendered(m_surfaceRenderedTimer.elapsed());
}

void CursorImage::update()
{
    using namespace KWayland::Server;
    disconnect(m_serverCursor.connection);
    auto p = waylandServer()->seat()->focusedPointer();
    if (p) {
        m_serverCursor.connection = connect(p, &PointerInterface::cursorChanged, this, &CursorImage::updateServerCursor);
    } else {
        m_serverCursor.connection = QMetaObject::Connection();
    }
    updateServerCursor();
}

void CursorImage::updateDecoration()
{
    disconnect(m_decorationConnection);
    auto deco = m_pointer->decoration();
    AbstractClient *c = deco.isNull() ? nullptr : deco->client();
    if (c) {
        m_decorationConnection = connect(c, &AbstractClient::moveResizeCursorChanged, this, &CursorImage::updateDecorationCursor);
    } else {
        m_decorationConnection = QMetaObject::Connection();
    }
    updateDecorationCursor();
}

void CursorImage::updateDecorationCursor()
{
    m_decorationCursor.image = QImage();
    m_decorationCursor.hotSpot = QPoint();

    auto deco = m_pointer->decoration();
    if (AbstractClient *c = deco.isNull() ? nullptr : deco->client()) {
        loadThemeCursor(c->cursor(), &m_decorationCursor);
        if (m_currentSource == CursorSource::Decoration) {
            emit changed();
        }
    }
    reevaluteSource();
}

void CursorImage::updateMoveResize()
{
    m_moveResizeCursor.image = QImage();
    m_moveResizeCursor.hotSpot = QPoint();
    if (AbstractClient *c = workspace()->getMovingClient()) {
        loadThemeCursor(c->isMove() ? Qt::SizeAllCursor : Qt::SizeBDiagCursor, &m_moveResizeCursor);
        if (m_currentSource == CursorSource::MoveResize) {
            emit changed();
        }
    }
    reevaluteSource();
}

void CursorImage::updateServerCursor()
{
    m_serverCursor.image = QImage();
    m_serverCursor.hotSpot = QPoint();
    reevaluteSource();
    const bool needsEmit = m_currentSource == CursorSource::LockScreen || m_currentSource == CursorSource::PointerSurface;
    auto p = waylandServer()->seat()->focusedPointer();
    if (!p) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto c = p->cursor();
    if (!c) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto buffer = cursorSurface.data()->buffer();
    if (!buffer) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    m_serverCursor.hotSpot = c->hotspot();
    m_serverCursor.image = buffer->data().copy();
    if (needsEmit) {
        emit changed();
    }
}

void CursorImage::loadTheme()
{
    if (m_cursorTheme) {
        return;
    }
    // check whether we can create it
    if (waylandServer()->internalShmPool()) {
        m_cursorTheme = new WaylandCursorTheme(waylandServer()->internalShmPool(), this);
        connect(waylandServer(), &WaylandServer::terminatingInternalClientConnection, this,
            [this] {
                delete m_cursorTheme;
                m_cursorTheme = nullptr;
            }
        );
    }
}

void CursorImage::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    loadThemeCursor(shape, &m_effectsCursor);
    if (m_currentSource == CursorSource::EffectsOverride) {
        emit changed();
    }
    reevaluteSource();
}

void CursorImage::removeEffectsOverrideCursor()
{
    reevaluteSource();
}

void CursorImage::updateDrag()
{
    using namespace KWayland::Server;
    disconnect(m_drag.connection);
    m_drag.cursor.image = QImage();
    m_drag.cursor.hotSpot = QPoint();
    reevaluteSource();
    if (auto p = waylandServer()->seat()->dragPointer()) {
        m_drag.connection = connect(p, &PointerInterface::cursorChanged, this, &CursorImage::updateDragCursor);
    } else {
        m_drag.connection = QMetaObject::Connection();
    }
    updateDragCursor();
}

void CursorImage::updateDragCursor()
{
    m_drag.cursor.image = QImage();
    m_drag.cursor.hotSpot = QPoint();
    const bool needsEmit = m_currentSource == CursorSource::DragAndDrop;
    QImage additionalIcon;
    if (auto ddi = waylandServer()->seat()->dragSource()) {
        if (auto dragIcon = ddi->icon()) {
            if (auto buffer = dragIcon->buffer()) {
                additionalIcon = buffer->data().copy();
            }
        }
    }
    auto p = waylandServer()->seat()->dragPointer();
    if (!p) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto c = p->cursor();
    if (!c) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto buffer = cursorSurface.data()->buffer();
    if (!buffer) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    m_drag.cursor.hotSpot = c->hotspot();
    m_drag.cursor.image = buffer->data().copy();
    if (needsEmit) {
        emit changed();
    }
    // TODO: add the cursor image
}

void CursorImage::loadThemeCursor(Qt::CursorShape shape, Image *image)
{
    loadTheme();
    if (!m_cursorTheme) {
        return;
    }
    auto it = m_cursors.constFind(shape);
    if (it == m_cursors.constEnd()) {
        image->image = QImage();
        image->hotSpot = QPoint();
        wl_cursor_image *cursor = m_cursorTheme->get(shape);
        if (!cursor) {
            return;
        }
        wl_buffer *b = wl_cursor_image_get_buffer(cursor);
        if (!b) {
            return;
        }
        waylandServer()->internalClientConection()->flush();
        waylandServer()->dispatch();
        auto buffer = KWayland::Server::BufferInterface::get(waylandServer()->internalConnection()->getResource(KWayland::Client::Buffer::getId(b)));
        if (!buffer) {
            return;
        }
        it = decltype(it)(m_cursors.insert(shape, {buffer->data().copy(), QPoint(cursor->hotspot_x, cursor->hotspot_y)}));
    }
    image->hotSpot = it.value().hotSpot;
    image->image = it.value().image;
}

void CursorImage::reevaluteSource()
{
    if (waylandServer()->seat()->isDragPointer()) {
        // TODO: touch drag?
        setSource(CursorSource::DragAndDrop);
        return;
    }
    if (waylandServer()->isScreenLocked()) {
        setSource(CursorSource::LockScreen);
        return;
    }
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->isMouseInterception()) {
        setSource(CursorSource::EffectsOverride);
        return;
    }
    if (workspace() && workspace()->getMovingClient()) {
        setSource(CursorSource::MoveResize);
        return;
    }
    if (!m_pointer->decoration().isNull()) {
        setSource(CursorSource::Decoration);
        return;
    }
    if (!m_pointer->window().isNull()) {
        setSource(CursorSource::PointerSurface);
        return;
    }
    setSource(CursorSource::Fallback);
}

void CursorImage::setSource(CursorSource source)
{
    if (m_currentSource == source) {
        return;
    }
    m_currentSource = source;
    emit changed();
}

QImage CursorImage::image() const
{
    switch (m_currentSource) {
    case CursorSource::EffectsOverride:
        return m_effectsCursor.image;
    case CursorSource::MoveResize:
        return m_moveResizeCursor.image;
    case CursorSource::LockScreen:
    case CursorSource::PointerSurface:
        // lockscreen also uses server cursor image
        return m_serverCursor.image;
    case CursorSource::Decoration:
        return m_decorationCursor.image;
    case CursorSource::DragAndDrop:
        return m_drag.cursor.image;
    case CursorSource::Fallback:
        return m_fallbackCursor.image;
    default:
        Q_UNREACHABLE();
    }
}

QPoint CursorImage::hotSpot() const
{
    switch (m_currentSource) {
    case CursorSource::EffectsOverride:
        return m_effectsCursor.hotSpot;
    case CursorSource::MoveResize:
        return m_moveResizeCursor.hotSpot;
    case CursorSource::LockScreen:
    case CursorSource::PointerSurface:
        // lockscreen also uses server cursor image
        return m_serverCursor.hotSpot;
    case CursorSource::Decoration:
        return m_decorationCursor.hotSpot;
    case CursorSource::DragAndDrop:
        return m_drag.cursor.hotSpot;
    case CursorSource::Fallback:
        return m_fallbackCursor.hotSpot;
    default:
        Q_UNREACHABLE();
    }
}

}
