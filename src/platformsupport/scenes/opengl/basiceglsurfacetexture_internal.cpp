/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platformsupport/scenes/opengl/basiceglsurfacetexture_internal.h"
#include "core/graphicsbufferview.h"
#include "libkwineffects/kwingltexture.h"
#include "scene/surfaceitem_internal.h"
#include "utils/common.h"

#include <QOpenGLFramebufferObject>

namespace KWin
{

BasicEGLSurfaceTextureInternal::BasicEGLSurfaceTextureInternal(OpenGLBackend *backend,
                                                               SurfacePixmapInternal *pixmap)
    : OpenGLSurfaceTextureInternal(backend, pixmap)
{
}

bool BasicEGLSurfaceTextureInternal::create()
{
    if (updateFromFramebuffer()) {
        return true;
    }

    GraphicsBuffer *buffer = m_pixmap->graphicsBuffer();
    if (!buffer) {
        qCDebug(KWIN_OPENGL) << "Failed to create surface texture for internal window";
        return false;
    }

    return updateFromImage(QRect(QPoint(0, 0), buffer->size()));
}

void BasicEGLSurfaceTextureInternal::update(const QRegion &region)
{
    if (updateFromFramebuffer()) {
        return;
    }
    if (m_pixmap->graphicsBuffer()) {
        updateFromImage(region);
        return;
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to update surface texture for internal window";
    }
}

bool BasicEGLSurfaceTextureInternal::updateFromFramebuffer()
{
    const QOpenGLFramebufferObject *fbo = m_pixmap->fbo();
    if (!fbo) {
        return false;
    }
    m_texture = GLTexture::createNonOwningWrapper(fbo->texture(), 0, fbo->size());
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_LINEAR);
    m_texture->setContentTransform(TextureTransforms());
    return true;
}

bool BasicEGLSurfaceTextureInternal::updateFromImage(const QRegion &region)
{
    const GraphicsBufferView view(m_pixmap->graphicsBuffer());
    if (view.isNull()) {
        return false;
    }

    if (!m_texture) {
        m_texture = GLTexture::upload(*view.image());
    } else {
        for (const QRect &rect : region) {
            m_texture->update(*view.image(), rect.topLeft(), rect);
        }
    }

    return m_texture != nullptr;
}

} // namespace KWin
