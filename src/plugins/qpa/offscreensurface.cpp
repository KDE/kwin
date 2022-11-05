/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "offscreensurface.h"
#include "core/outputbackend.h"
#include "eglhelpers.h"
#include "main.h"

#include <QOffscreenSurface>

namespace KWin
{
namespace QPA
{

OffscreenSurface::OffscreenSurface(QOffscreenSurface *surface)
    : QPlatformOffscreenSurface(surface)
    , m_eglDisplay(kwinApp()->outputBackend()->sceneEglDisplay())
{
    const QSize size = surface->size();

    EGLConfig config = configFromFormat(m_eglDisplay, surface->requestedFormat(), EGL_PBUFFER_BIT);
    if (config == EGL_NO_CONFIG_KHR) {
        return;
    }

    const EGLint attributes[] = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_NONE};

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

EGLSurface OffscreenSurface::eglSurface() const
{
    return m_surface;
}

} // namespace QPA
} // namespace KWin
