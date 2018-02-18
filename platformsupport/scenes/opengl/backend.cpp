/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#include "backend.h"
#include <kwineffects.h>
#include <logging.h>

#include "screens.h"

#include <epoxy/gl.h>

namespace KWin
{

OpenGLBackend::OpenGLBackend()
    : m_syncsToVBlank(false)
    , m_blocksForRetrace(false)
    , m_directRendering(false)
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

void OpenGLBackend::idle()
{
    if (hasPendingFlush()) {
        effects->makeOpenGLContextCurrent();
        present();
    }
}

void OpenGLBackend::addToDamageHistory(const QRegion &region)
{
    if (m_damageHistory.count() > 10)
        m_damageHistory.removeLast();

    m_damageHistory.prepend(region);
}

QRegion OpenGLBackend::accumulatedDamageHistory(int bufferAge) const
{
    QRegion region;

    // Note: An age of zero means the buffer contents are undefined
    if (bufferAge > 0 && bufferAge <= m_damageHistory.count()) {
        for (int i = 0; i < bufferAge - 1; i++)
            region |= m_damageHistory[i];
    } else {
        const QSize &s = screens()->size();
        region = QRegion(0, 0, s.width(), s.height());
    }

    return region;
}

OverlayWindow* OpenGLBackend::overlayWindow()
{
    return NULL;
}

QRegion OpenGLBackend::prepareRenderingForScreen(int screenId)
{
    // fallback to repaint complete screen
    return screens()->geometry(screenId);
}

void OpenGLBackend::endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion)
{
    Q_UNUSED(screenId)
    Q_UNUSED(damage)
    Q_UNUSED(damagedRegion)
}

bool OpenGLBackend::perScreenRendering() const
{
    return false;
}

void OpenGLBackend::copyPixels(const QRegion &region)
{
    const int height = screens()->size().height();
    foreach (const QRect &r, region.rects()) {
        const int x0 = r.x();
        const int y0 = height - r.y() - r.height();
        const int x1 = r.x() + r.width();
        const int y1 = height - r.y();

        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

QVector<uint32_t> OpenGLBackend::supportedDrmFormats()
{
    return QVector<uint32_t>();
}

QVector<uint64_t> OpenGLBackend::supportedDrmModifiers(uint32_t format)
{
    Q_UNUSED(format)

    return QVector<uint64_t>();
}

KWayland::Server::LinuxDmabuf::Buffer *OpenGLBackend::importDmabufBuffer(const QVector<KWayland::Server::LinuxDmabuf::Plane> &planes,
                                                                         uint32_t format,
                                                                         const QSize &size,
                                                                         KWayland::Server::LinuxDmabuf::Flags flags)
{
    Q_UNUSED(planes)
    Q_UNUSED(format)
    Q_UNUSED(size)
    Q_UNUSED(flags)

    return nullptr;
}


}
