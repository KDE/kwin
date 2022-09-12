/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "surfaceitem_internal.h"
#include "composite.h"
#include "internalwindow.h"
#include "scene.h"

#include <QOpenGLFramebufferObject>

namespace KWin
{

SurfaceItemInternal::SurfaceItemInternal(InternalWindow *window, Item *parent)
    : SurfaceItem(window, parent)
{
    connect(window, &Window::bufferGeometryChanged,
            this, &SurfaceItemInternal::handleBufferGeometryChanged);

    setSize(window->bufferGeometry().size());

    // The device pixel ratio of the internal window is static.
    QMatrix4x4 surfaceToBufferMatrix;
    surfaceToBufferMatrix.scale(window->bufferScale());
    setSurfaceToBufferMatrix(surfaceToBufferMatrix);
}

QVector<QRectF> SurfaceItemInternal::shape() const
{
    return {rect()};
}

std::unique_ptr<SurfacePixmap> SurfaceItemInternal::createPixmap()
{
    return std::make_unique<SurfacePixmapInternal>(this);
}

void SurfaceItemInternal::handleBufferGeometryChanged(Window *window, const QRectF &old)
{
    if (window->bufferGeometry().size() != old.size()) {
        discardPixmap();
    }
    setSize(window->bufferGeometry().size());
}

SurfacePixmapInternal::SurfacePixmapInternal(SurfaceItemInternal *item, QObject *parent)
    : SurfacePixmap(Compositor::self()->scene()->createSurfaceTextureInternal(this), parent)
    , m_item(item)
{
}

QOpenGLFramebufferObject *SurfacePixmapInternal::fbo() const
{
    return m_fbo.get();
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
    const Window *window = m_item->window();

    if (window->internalFramebufferObject()) {
        m_fbo = window->internalFramebufferObject();
        m_size = m_fbo->size();
        m_hasAlphaChannel = true;
    } else if (!window->internalImageObject().isNull()) {
        m_rasterBuffer = window->internalImageObject();
        m_size = m_rasterBuffer.size();
        m_hasAlphaChannel = m_rasterBuffer.hasAlphaChannel();
    }
}

bool SurfacePixmapInternal::isValid() const
{
    return m_fbo != nullptr || !m_rasterBuffer.isNull();
}

} // namespace KWin
