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
#include "abstract_backend.h"
#include "effects.h"
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
    : QObject(parent)
    , m_input(parent)
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
    connect(m_cursor, &CursorImage::changed, waylandServer()->backend(), &AbstractBackend::cursorChanged);
    emit m_cursor->changed();
    connect(workspace(), &Workspace::stackingOrderChanged, this, &PointerInputRedirection::update);
    connect(screens(), &Screens::changed, this, &PointerInputRedirection::updateAfterScreenChange);
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &PointerInputRedirection::update);
    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });

    // warp the cursor to center of screen
    warp(screens()->geometry().center());
    updateAfterScreenChange();
}

void PointerInputRedirection::processMotion(const QPointF &pos, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    updatePosition(pos);
    QMouseEvent event(QEvent::MouseMove, m_pos.toPoint(), m_pos.toPoint(),
                      Qt::NoButton, m_qtButtons, m_input->keyboardModifiers());
    event.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, 0)) {
            return;
        }
    }
}

void PointerInputRedirection::processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    updateButton(button, state);

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

    QMouseEvent event(type, m_pos.toPoint(), m_pos.toPoint(),
                      buttonToQtMouseButton(button), m_qtButtons, m_input->keyboardModifiers());
    event.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, button)) {
            return;
        }
    }
}

void PointerInputRedirection::processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    if (delta == 0) {
        return;
    }

    emit m_input->pointerAxisChanged(axis, delta);

    QWheelEvent wheelEvent(m_pos, m_pos, QPoint(),
                           (axis == InputRedirection::PointerAxisHorizontal) ? QPoint(delta, 0) : QPoint(0, delta),
                           delta,
                           (axis == InputRedirection::PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                           m_qtButtons,
                           m_input->keyboardModifiers());
    wheelEvent.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->wheelEvent(&wheelEvent)) {
            return;
        }
    }
}

void PointerInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    // TODO: handle pointer grab aka popups
    Toplevel *t = m_input->findToplevel(m_pos.toPoint());
    const auto oldDeco = m_decoration;
    updateInternalWindow();
    if (!m_internalWindow) {
        updateDecoration(t);
    } else {
        // TODO: send hover leave to decoration
        if (m_decoration) {
            m_decoration->client()->leaveEvent();
        }
        m_decoration.clear();
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
        seat->setFocusedPointerSurface(nullptr);
        t = nullptr;
    }
}

void PointerInputRedirection::updateInternalWindow()
{
    const auto oldInternalWindow = m_internalWindow;
    bool found = false;
    // TODO: screen locked check without going through wayland server
    bool needsReset = waylandServer()->isScreenLocked();
    const auto &internalClients = waylandServer()->internalClients();
    const bool change = m_internalWindow.isNull() || !(m_internalWindow->flags().testFlag(Qt::Popup) && m_internalWindow->isVisible());
    if (!internalClients.isEmpty() && change) {
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                if (w->geometry().contains(m_pos.toPoint())) {
                    m_internalWindow = QPointer<QWindow>(w);
                    found = true;
                    break;
                }
            }
        } while (it != internalClients.begin());
        if (!found) {
            needsReset = true;
        }
    }
    if (needsReset) {
        m_internalWindow.clear();
    }
    if (oldInternalWindow != m_internalWindow) {
        // changed
        if (oldInternalWindow) {
            disconnect(m_internalWindowConnection);
            m_internalWindowConnection = QMetaObject::Connection();
            QEvent event(QEvent::Leave);
            QCoreApplication::sendEvent(oldInternalWindow.data(), &event);
        }
        if (m_internalWindow) {
            m_internalWindowConnection = connect(m_internalWindow.data(), &QWindow::visibleChanged, this,
                [this] (bool visible) {
                    if (!visible) {
                        update();
                    }
                });
            QEnterEvent event(m_pos - m_internalWindow->position(),
                              m_pos - m_internalWindow->position(),
                              m_pos);
            QCoreApplication::sendEvent(m_internalWindow.data(), &event);
            return;
        }
    }
}

void PointerInputRedirection::updateDecoration(Toplevel *t)
{
    const auto oldDeco = m_decoration;
    bool needsReset = waylandServer()->isScreenLocked();
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
        // check whether it's on a Decoration
        if (c->decoratedClient()) {
            const QRect clientRect = QRect(c->clientPos(), c->clientSize()).translated(c->pos());
            if (!clientRect.contains(m_pos.toPoint())) {
                m_decoration = c->decoratedClient();
            } else {
                needsReset = true;
            }
        } else {
            needsReset = true;
        }
    } else {
        needsReset = true;
    }
    if (needsReset) {
        m_decoration.clear();
    }

    bool leftSend = false;
    auto oldWindow = qobject_cast<AbstractClient*>(m_window.data());
    if (oldWindow && (m_decoration && m_decoration->client() != oldWindow)) {
        leftSend = true;
        oldWindow->leaveEvent();
    }

    if (oldDeco && oldDeco != m_decoration) {
        if (oldDeco->client() != t && !leftSend) {
            leftSend = true;
            oldDeco->client()->leaveEvent();
        }
        // send leave
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(oldDeco->decoration(), &event);
    }
    if (m_decoration) {
        if (m_decoration->client() != oldWindow) {
            m_decoration->client()->enterEvent(m_pos.toPoint());
            workspace()->updateFocusMousePosition(m_pos.toPoint());
        }
        const QPointF p = m_pos - t->pos();
        QHoverEvent event(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(m_decoration->decoration(), &event);
        m_decoration->client()->processDecorationMove();
    }
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
        waylandServer()->backend()->warpPointer(pos);
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
    if (waylandServer()->backend()->supportsPointerWarping()) {
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
    m_cursor->setEffectsOverrideCursor(shape);
}

void PointerInputRedirection::removeEffectsOverrideCursor()
{
    if (!m_inited) {
        return;
    }
    m_cursor->removeEffectsOverrideCursor();
}

CursorImage::CursorImage(PointerInputRedirection *parent)
    : QObject(parent)
    , m_pointer(parent)
{
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::focusedPointerChanged, this, &CursorImage::update);
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &CursorImage::reevaluteSource);
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
    case CursorSource::Fallback:
        return m_fallbackCursor.hotSpot;
    default:
        Q_UNREACHABLE();
    }
}

}
