/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_internal.h"
#include "composite.h"
#include "internal_client.h"
#include "scene.h"

namespace KWin
{

SurfaceItemInternal::SurfaceItemInternal(InternalClient *window, Item *parent)
    : SurfaceItem(window, parent)
{
    connect(window, &Toplevel::bufferGeometryChanged,
            this, &SurfaceItemInternal::handleBufferGeometryChanged);

    setSize(window->bufferGeometry().size());

    // The device pixel ratio of the internal window is static.
    QMatrix4x4 surfaceToBufferMatrix;
    surfaceToBufferMatrix.scale(window->bufferScale());
    setSurfaceToBufferMatrix(surfaceToBufferMatrix);
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
    : SurfacePixmap(Compositor::self()->scene()->createSurfaceTextureInternal(this), parent)
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
    const Toplevel *window = m_item->window();

    if (window->internalFramebufferObject()) {
        m_fbo = window->internalFramebufferObject();
        m_hasAlphaChannel = true;
    } else if (!window->internalImageObject().isNull()) {
        m_rasterBuffer = window->internalImageObject();
        m_hasAlphaChannel = m_rasterBuffer.hasAlphaChannel();
    }
}

bool SurfacePixmapInternal::isValid() const
{
    return !m_fbo.isNull() || !m_rasterBuffer.isNull();
}

} // namespace KWin
