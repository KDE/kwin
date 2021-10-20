/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surface.h"
#include "deleted.h"

namespace KWin
{

Surface::Surface(Toplevel *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    connect(window, &Toplevel::windowClosed, this, &Surface::handleWindowClosed);
}

void Surface::handleWindowClosed(Toplevel *toplevel, Deleted *deleted)
{
    Q_UNUSED(toplevel)
    m_window = deleted;
}

Toplevel *Surface::window() const
{
    return m_window;
}

QRegion Surface::shape() const
{
    return QRegion();
}

QRegion Surface::opaque() const
{
    return QRegion();
}

QMatrix4x4 Surface::surfaceToBufferMatrix() const
{
    return m_surfaceToBufferMatrix;
}

void Surface::setSurfaceToBufferMatrix(const QMatrix4x4 &matrix)
{
    m_surfaceToBufferMatrix = matrix;
}

QPoint Surface::position() const
{
    return m_position;
}

void Surface::setPosition(const QPoint &pos)
{
    if (m_position != pos) {
        m_position = pos;
        Q_EMIT positionChanged();
    }
}

QSize Surface::size() const
{
    return m_size;
}

void Surface::setSize(const QSize &size)
{
    if (m_size != size) {
        m_size = size;
        Q_EMIT sizeChanged();
    }
}

QList<Surface *> Surface::below() const
{
    return m_below;
}

void Surface::setBelow(const QList<Surface *> &below)
{
    if (m_below != below) {
        m_below = below;
        Q_EMIT belowChanged();
    }
}

QList<Surface *> Surface::above() const
{
    return m_above;
}

void Surface::setAbove(const QList<Surface *> &above)
{
    if (m_above != above) {
        m_above = above;
        Q_EMIT aboveChanged();
    }
}

bool Surface::isMapped() const
{
    return m_mapped;
}

void Surface::setMapped(bool mapped)
{
    if (m_mapped != mapped) {
        m_mapped = mapped;
        Q_EMIT mappedChanged();
    }
}

void Surface::addView(SurfaceView *view)
{
    m_views.append(view);
}

void Surface::removeView(SurfaceView *view)
{
    m_views.removeOne(view);
}

void Surface::frame()
{
    for (SurfaceView *view : qAsConst(m_views)) {
        view->surfaceFrame();
    }
}

void Surface::discardQuads()
{
    for (SurfaceView *view : qAsConst(m_views)) {
        view->surfaceQuadsInvalidated();
    }
}

void Surface::discardPixmap()
{
    for (SurfaceView *view : qAsConst(m_views)) {
        view->surfacePixmapInvalidated();
    }
}

SurfaceView::SurfaceView(Surface *surface)
    : m_surface(surface)
{
    m_surface->addView(this);
}

SurfaceView::~SurfaceView()
{
    if (m_surface) {
        m_surface->removeView(this);
    }
}

Surface *SurfaceView::surface() const
{
    return m_surface;
}

SurfaceTexture::~SurfaceTexture()
{
}

SurfacePixmap::SurfacePixmap(SurfaceTexture *texture, QObject *parent)
    : QObject(parent)
    , m_texture(texture)
{
}

void SurfacePixmap::update()
{
}

SurfaceTexture *SurfacePixmap::texture() const
{
    return m_texture.data();
}

bool SurfacePixmap::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

QSize SurfacePixmap::size() const
{
    return m_size;
}

bool SurfacePixmap::isDiscarded() const
{
    return m_isDiscarded;
}

void SurfacePixmap::markAsDiscarded()
{
    m_isDiscarded = true;
}

} // namespace KWin
