/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "openglbackend.h"
#include <kwineffects.h>
#include <kwinglutils_funcs.h>

#include "screens.h"
#include "utils/common.h"

#include <QElapsedTimer>

#include <unistd.h>

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

CompositingType OpenGLBackend::compositingType() const
{
    return OpenGLCompositing;
}

void OpenGLBackend::setFailed(const QString &reason)
{
    qCWarning(KWIN_OPENGL) << "Creating the OpenGL rendering failed: " << reason;
    m_failed = true;
}

bool OpenGLBackend::scanout(AbstractOutput *output, SurfaceItem *surfaceItem)
{
    Q_UNUSED(output)
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

void OpenGLBackend::aboutToStartPainting(AbstractOutput *output, const QRegion &damage)
{
    Q_UNUSED(output)
    Q_UNUSED(damage)
}


bool OpenGLBackend::directScanoutAllowed(AbstractOutput *output) const
{
    Q_UNUSED(output);
    return false;
}

SurfaceTexture *OpenGLBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *OpenGLBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

SurfaceTexture *OpenGLBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    Q_UNUSED(pixmap)
    return nullptr;
}

bool OpenGLBackend::checkGraphicsReset()
{
    const GLenum status = KWin::glGetGraphicsResetStatus();
    if (Q_LIKELY(status == GL_NO_ERROR)) {
        return false;
    }

    switch (status) {
    case GL_GUILTY_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset attributable to the current GL context occurred.";
        break;
    case GL_INNOCENT_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset not attributable to the current GL context occurred.";
        break;
    case GL_UNKNOWN_CONTEXT_RESET:
        qCDebug(KWIN_OPENGL) << "A graphics reset of an unknown cause occurred.";
        break;
    default:
        break;
    }

    QElapsedTimer timer;
    timer.start();

    // Wait until the reset is completed or max 10 seconds
    while (timer.elapsed() < 10000 && KWin::glGetGraphicsResetStatus() != GL_NO_ERROR) {
        usleep(50);
    }

    return true;
}

}
