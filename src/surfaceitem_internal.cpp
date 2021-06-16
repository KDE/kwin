/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_internal.h"
#include "composite.h"
#include "scene.h"

namespace KWin
{

SurfaceItemInternal::SurfaceItemInternal(Scene::Window *window, Item *parent)
    : SurfaceItem(window, parent)
{
    Toplevel *toplevel = window->window();

    connect(toplevel, &Toplevel::bufferGeometryChanged,
            this, &SurfaceItemInternal::handleBufferGeometryChanged);

    setSize(toplevel->bufferGeometry().size());
}

QPointF SurfaceItemInternal::mapToBuffer(const QPointF &point) const
{
    return point * window()->window()->bufferScale();
}

QRegion SurfaceItemInternal::shape() const
{
    return QRegion(rect());
}

SurfacePixmap *SurfaceItemInternal::createPixmap()
{
    return new SurfacePixmapInternal(this);
}

void SurfaceItemInternal::handleBufferGeometryChanged(Toplevel *toplevel, const QRect &old)
{
    if (toplevel->bufferGeometry().size() != old.size()) {
        discardPixmap();
    }
    setSize(toplevel->bufferGeometry().size());
}

SurfacePixmapInternal::SurfacePixmapInternal(SurfaceItemInternal *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createSurfaceTextureProviderInternal(this), parent)
    , m_item(item)
{
}

QOpenGLFramebufferObject *SurfacePixmapInternal::fbo() const
{
    return m_fbo.data();
}

QImage SurfacePixmapInternal::image() const
{
    return m_rasterBuffer;
}

void SurfacePixmapInternal::create()
{
    update();
}

void SurfacePixmapInternal::update()
{
    const Toplevel *toplevel = m_item->window()->window();

    if (toplevel->internalFramebufferObject()) {
        m_fbo = toplevel->internalFramebufferObject();
        m_hasAlphaChannel = true;
    } else if (!toplevel->internalImageObject().isNull()) {
        m_rasterBuffer = toplevel->internalImageObject();
        m_hasAlphaChannel = m_rasterBuffer.hasAlphaChannel();
    }
}

bool SurfacePixmapInternal::isValid() const
{
    return !m_fbo.isNull() || !m_rasterBuffer.isNull();
}

} // namespace KWin
