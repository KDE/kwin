/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "basiceglsurfacetexture_internal.h"
#include "kwingltexture.h"
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
    } else if (updateFromImage(m_pixmap->image().rect())) {
        return true;
    } else {
        qCDebug(KWIN_OPENGL) << "Failed to create surface texture for internal window";
        return false;
    }
}

void BasicEGLSurfaceTextureInternal::update(const QRegion &region)
{
    if (updateFromFramebuffer()) {
        return;
    } else if (updateFromImage(region)) {
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
    m_texture.reset(new GLTexture(fbo->texture(), 0, fbo->size()));
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setFilter(GL_LINEAR);
    m_texture->setYInverted(false);
    return true;
}

static QRegion scale(const QRegion &region, qreal scaleFactor)
{
    if (scaleFactor == 1) {
        return region;
    }

    QRegion scaled;
    for (const QRect &rect : region) {
        scaled += QRect(rect.topLeft() * scaleFactor, rect.size() * scaleFactor);
    }
    return scaled;
}

bool BasicEGLSurfaceTextureInternal::updateFromImage(const QRegion &region)
{
    const QImage image = m_pixmap->image();
    if (image.isNull()) {
        return false;
    }

    if (!m_texture) {
        m_texture.reset(new GLTexture(image));
    } else {
        const QRegion nativeRegion = scale(region, image.devicePixelRatio());
        for (const QRect &rect : nativeRegion) {
            m_texture->update(image, rect.topLeft(), rect);
        }
    }

    return true;
}

} // namespace KWin
