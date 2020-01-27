/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

#include "cursorimage.h"
#include "abstract_client.h"
#include "decorations/decoratedclient.h"
#include "effects.h"
#include "pointer_input.h"
#include "screens.h"
#include "x11client.h"
#include "xdgshellclient.h"
#include "wayland_cursor_theme.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KScreenLocker/KsldApp>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/buffer.h>

#include <wayland-cursor.h>

using namespace KWin;

CursorImage::CursorImage(QObject *parent)
    : QObject(parent)
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
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this] (AbstractClient *c) {
        connect(c, &AbstractClient::moveResizedChanged, this, &CursorImage::updateMoveResize);
        connect(c, &AbstractClient::moveResizeCursorChanged, this, &CursorImage::updateMoveResize);
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
                m_cursorsByName.clear();
                loadThemeCursor(Qt::ArrowCursor, &m_fallbackCursor);
                updateDecorationCursor(input()->pointer()->decoration());
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

    auto surface = cursorSurface();
    if (surface.isNull()) {
        return;
    }
    surface->frameRendered(m_surfaceRenderedTimer.elapsed());
}

void CursorImage::update()
{
    if (m_blockUpdate) {
        return;
    }
    using namespace KWayland::Server;
    disconnect(m_serverCursor.connection);
    auto p = waylandServer()->seat()->focusedPointer();
    if (p) {
        m_serverCursor.connection = connect(p, &PointerInterface::cursorChanged, this, &CursorImage::updateServerCursor);
    } else {
        m_serverCursor.connection = QMetaObject::Connection();
        reevaluteSource();
    }
}

void CursorImage::updateDecoration(Decoration::DecoratedClientImpl* deco)
{
    disconnect(m_decorationConnection);
    AbstractClient *c = !deco ? nullptr : deco->client();
    if (c) {
        m_decorationConnection = connect(c, &AbstractClient::moveResizeCursorChanged, this, &CursorImage::updateDecorationCursorWithShape);
    } else {
        m_decorationConnection = QMetaObject::Connection();
    }
    updateDecorationCursor(deco);
}

void CursorImage::updateDecorationCursor(Decoration::DecoratedClientImpl* deco)
{
    m_decorationCursor.image = QImage();
    m_decorationCursor.hotSpot = QPoint();

    if (AbstractClient *c = !deco ? nullptr : deco->client()) {
        loadThemeCursor(c->cursor(), &m_decorationCursor);
        if (m_currentSource == CursorSource::Decoration) {
            emit changed();
        }
    }
    reevaluteSource();
}

void CursorImage::updateDecorationCursorWithShape(CursorShape shape, Decoration::DecoratedClientImpl* decoration)
{
    Q_UNUSED(shape);
    updateDecorationCursor(decoration);
}

void CursorImage::updateMoveResize()
{
    m_moveResizeCursor.image = QImage();
    m_moveResizeCursor.hotSpot = QPoint();
    if (AbstractClient *c = workspace()->moveResizeClient()) {
        loadThemeCursor(c->cursor(), &m_moveResizeCursor);
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

    auto surface = cursorSurface();
    if (surface.isNull()) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto buffer = surface.data()->buffer();
    if (!buffer) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    m_serverCursor.hotSpot = cursorHotspot();
    m_serverCursor.image = buffer->data().copy();
    m_serverCursor.image.setDevicePixelRatio(surface->scale());
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

void CursorImage::setWindowSelectionCursor(const QByteArray &shape)
{
    if (shape.isEmpty()) {
        loadThemeCursor(Qt::CrossCursor, &m_windowSelectionCursor);
    } else {
        loadThemeCursor(shape, &m_windowSelectionCursor);
    }
    if (m_currentSource == CursorSource::WindowSelector) {
        emit changed();
    }
    reevaluteSource();
}

void CursorImage::removeWindowSelectionCursor()
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

void CursorImage::loadThemeCursor(CursorShape shape, Image *image)
{
    loadThemeCursor(shape, m_cursors, image);
}

void CursorImage::loadThemeCursor(const QByteArray &shape, Image *image)
{
    loadThemeCursor(shape, m_cursorsByName, image);
}

template <typename T>
void CursorImage::loadThemeCursor(const T &shape, QHash<T, Image> &cursors, Image *image)
{
    loadTheme();
    if (!m_cursorTheme) {
        return;
    }
    auto it = cursors.constFind(shape);
    if (it == cursors.constEnd()) {
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
        auto scale = screens()->maxScale();
        int hotSpotX = qRound(cursor->hotspot_x / scale);
        int hotSpotY = qRound(cursor->hotspot_y / scale);
        QImage img = buffer->data().copy();
        img.setDevicePixelRatio(scale);
        it = decltype(it)(cursors.insert(shape, {img, QPoint(hotSpotX, hotSpotY)}));
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
    if (input()->isSelectingWindow()) {
        setSource(CursorSource::WindowSelector);
        return;
    }
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->isMouseInterception()) {
        setSource(CursorSource::EffectsOverride);
        return;
    }
    if (workspace() && workspace()->moveResizeClient()) {
        setSource(CursorSource::MoveResize);
        return;
    }
    if (!input()->pointer()->decoration().isNull()) {
        setSource(CursorSource::Decoration);
        return;
    }
    if (!input()->pointer()->focus().isNull() && waylandServer()->seat()->focusedPointer()) {
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
    case CursorSource::WindowSelector:
        return m_windowSelectionCursor.image;
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
    case CursorSource::WindowSelector:
        return m_windowSelectionCursor.hotSpot;
    default:
        Q_UNREACHABLE();
    }
}
