/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "window.h"
#include "backingstore.h"
#include "../../wayland_server.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

namespace KWin
{
namespace QPA
{

BackingStore::BackingStore(QWindow *w, KWayland::Client::ShmPool *shm)
    : QPlatformBackingStore(w)
    , m_shm(shm)
    , m_backBuffer(QSize(), QImage::Format_ARGB32_Premultiplied)
{
    QObject::connect(m_shm, &KWayland::Client::ShmPool::poolResized,
        [this] {
            if (!m_buffer) {
                return;
            }
            auto b = m_buffer.toStrongRef();
            if (!b->isUsed()){
                return;
            }
            const QSize size = m_backBuffer.size();
            m_backBuffer = QImage(b->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
        }
    );
}

BackingStore::~BackingStore() = default;

QPaintDevice *BackingStore::paintDevice()
{
    return &m_backBuffer;
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents)
    m_size = size;
    if (!m_buffer) {
        return;
    }
    m_buffer.toStrongRef()->setUsed(false);
    m_buffer.clear();
}

void BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(region)
    Q_UNUSED(offset)
    auto s = static_cast<Window *>(window->handle())->surface();
    if (!s) {
        return;
    }
    s->attachBuffer(m_buffer);
    // TODO: proper damage region
    s->damage(QRect(QPoint(0, 0), m_backBuffer.size()));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    waylandServer()->internalClientConection()->flush();
    waylandServer()->dispatch();
}

void BackingStore::beginPaint(const QRegion&)
{
    if (m_buffer) {
        auto b = m_buffer.toStrongRef();
        if (b->isReleased()) {
            // we can re-use this buffer
            b->setReleased(false);
            return;
        } else {
            // buffer is still in use, get a new one
            b->setUsed(false);
        }
    }
    auto oldBuffer = m_buffer.toStrongRef();
    m_buffer.clear();
    m_buffer = m_shm->getBuffer(m_size, m_size.width() * 4);
    if (!m_buffer) {
        m_backBuffer = QImage();
        return;
    }
    auto b = m_buffer.toStrongRef();
    b->setUsed(true);
    m_backBuffer = QImage(b->address(), m_size.width(), m_size.height(), QImage::Format_ARGB32_Premultiplied);
    if (oldBuffer) {
        b->copy(oldBuffer->address());
    } else {
        m_backBuffer.fill(Qt::transparent);
    }
}

}
}
