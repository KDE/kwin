/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Vlad Zagorodniy <vladzzag@gmail.com>

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

#include "offscreensurface.h"
#include "eglhelpers.h"
#include "main.h"
#include "platform.h"

#include <QOffscreenSurface>

namespace KWin
{
namespace QPA
{

OffscreenSurface::OffscreenSurface(QOffscreenSurface *surface)
    : QPlatformOffscreenSurface(surface)
    , m_eglDisplay(kwinApp()->platform()->sceneEglDisplay())
{
    const QSize size = surface->size();

    EGLConfig config = configFromFormat(m_eglDisplay, surface->requestedFormat(), EGL_PBUFFER_BIT);
    if (config == EGL_NO_CONFIG_KHR) {
        return;
    }

    const EGLint attributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_NONE
    };

    m_surface = eglCreatePbufferSurface(m_eglDisplay, config, attributes);
    if (m_surface == EGL_NO_SURFACE) {
        return;
    }

    // Requested and actual surface format might be different.
    m_format = formatFromConfig(m_eglDisplay, config);
}

OffscreenSurface::~OffscreenSurface()
{
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_eglDisplay, m_surface);
    }
}

QSurfaceFormat OffscreenSurface::format() const
{
    return m_format;
}

bool OffscreenSurface::isValid() const
{
    return m_surface != EGL_NO_SURFACE;
}

EGLSurface OffscreenSurface::nativeHandle() const
{
    return m_surface;
}

} // namespace QPA
} // namespace KWin
