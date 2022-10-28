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

#include "surfaceitem.h"
#include "utils/common.h"
#include "workspace.h"

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

void OpenGLBackend::copyPixels(const QRegion &region, const QSize &screenSize)
{
    const int height = screenSize.height();
    for (const QRect &r : region) {
        const int x0 = r.x();
        const int y0 = height - r.y() - r.height();
        const int x1 = r.x() + r.width();
        const int y1 = height - r.y();

        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

std::shared_ptr<KWin::GLTexture> OpenGLBackend::textureForOutput(Output *output) const
{
    return {};
}

std::unique_ptr<SurfaceTexture> OpenGLBackend::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> OpenGLBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return nullptr;
}

std::unique_ptr<SurfaceTexture> OpenGLBackend::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
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
