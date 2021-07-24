/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "openglbackend.h"
#include <kwineffects.h>
#include <logging.h>

#include "screens.h"

#include <epoxy/gl.h>

namespace KWin
{

OpenGLBackend::OpenGLBackend()
    : m_directRendering(false)
    , m_haveBufferAge(false)
    , m_failed(false)
{
}

OpenGLBackend::~OpenGLBackend()
{
}

void OpenGLBackend::setFailed(const QString &reason)
{
    qCWarning(KWIN_OPENGL) << "Creating the OpenGL rendering failed: " << reason;
    m_failed = true;
}

OverlayWindow* OpenGLBackend::overlayWindow() const
{
    return nullptr;
}

bool OpenGLBackend::scanout(int screenId, SurfaceItem *surfaceItem)
{
    Q_UNUSED(screenId)
    Q_UNUSED(surfaceItem)
    return false;
}

void OpenGLBackend::copyPixels(const QRegion &region)
{
    const int height = screens()->size().height();
    for (const QRect &r : region) {
        const int x0 = r.x();
        const int y0 = height - r.y() - r.height();
        const int x1 = r.x() + r.width();
        const int y1 = height - r.y();

        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

QSharedPointer<KWin::GLTexture> OpenGLBackend::textureForOutput(AbstractOutput* output) const
{
    Q_UNUSED(output)
    return {};
}

void OpenGLBackend::aboutToStartPainting(int screenId, const QRegion &damage)
{
    Q_UNUSED(screenId)
    Q_UNUSED(damage)
}


bool OpenGLBackend::directScanoutAllowed(int screen) const
{
    Q_UNUSED(screen);
    return false;
}

PlatformSurfaceTexture *OpenGLBackend::createPlatformSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

PlatformSurfaceTexture *OpenGLBackend::createPlatformSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

PlatformSurfaceTexture *OpenGLBackend::createPlatformSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

}
