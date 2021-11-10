/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "drmclientbuffer.h"
#include "clientbuffer_p.h"
#include "display.h"

#include <EGL/egl.h>
#include <QtGui/qopengl.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_Y_INVERTED_WL 0x31DB
#endif

namespace KWaylandServer
{
typedef EGLBoolean (*eglQueryWaylandBufferWL_func)(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
static eglQueryWaylandBufferWL_func eglQueryWaylandBufferWL = nullptr;

class DrmClientBufferPrivate : public ClientBufferPrivate
{
public:
    int textureFormat = 0;
    int width = 0;
    int height = 0;
    int yInverted = 0;
    bool hasAlphaChannel = false;
};

DrmClientBuffer::DrmClientBuffer(wl_resource *resource, DrmClientBufferIntegration *integration)
    : ClientBuffer(resource, *new DrmClientBufferPrivate)
{
    Q_D(DrmClientBuffer);

    EGLDisplay eglDisplay = integration->display()->eglDisplay();
    eglQueryWaylandBufferWL(eglDisplay, resource, EGL_TEXTURE_FORMAT, &d->textureFormat);
    eglQueryWaylandBufferWL(eglDisplay, resource, EGL_WIDTH, &d->width);
    eglQueryWaylandBufferWL(eglDisplay, resource, EGL_HEIGHT, &d->height);

    if (!eglQueryWaylandBufferWL(eglDisplay, resource, EGL_WAYLAND_Y_INVERTED_WL, &d->yInverted)) {
        // If EGL_WAYLAND_Y_INVERTED_WL is unsupported, we must assume that the buffer is inverted.
        d->yInverted = true;
    }
}

int DrmClientBuffer::textureFormat() const
{
    Q_D(const DrmClientBuffer);
    return d->textureFormat;
}

QSize DrmClientBuffer::size() const
{
    Q_D(const DrmClientBuffer);
    return QSize(d->width, d->height);
}

bool DrmClientBuffer::hasAlphaChannel() const
{
    Q_D(const DrmClientBuffer);
    return d->textureFormat == EGL_TEXTURE_RGBA;
}

ClientBuffer::Origin DrmClientBuffer::origin() const
{
    Q_D(const DrmClientBuffer);
    return d->yInverted ? Origin::TopLeft : Origin::BottomLeft;
}

DrmClientBufferIntegration::DrmClientBufferIntegration(Display *display)
    : ClientBufferIntegration(display)
{
}

ClientBuffer *DrmClientBufferIntegration::createBuffer(::wl_resource *resource)
{
    EGLDisplay eglDisplay = display()->eglDisplay();
    static bool resolved = false;
    if (!resolved && eglDisplay != EGL_NO_DISPLAY) {
        eglQueryWaylandBufferWL = (eglQueryWaylandBufferWL_func)eglGetProcAddress("eglQueryWaylandBufferWL");
        resolved = true;
    }

    EGLint format;
    if (eglQueryWaylandBufferWL(eglDisplay, resource, EGL_TEXTURE_FORMAT, &format)) {
        return new DrmClientBuffer(resource, this);
    }
    return nullptr;
}

} // namespace KWaylandServer
