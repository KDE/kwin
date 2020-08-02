/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "sharingplatformcontext.h"
#include "offscreensurface.h"
#include "window.h"

#include "../../internal_client.h"
#include "../../main.h"
#include "../../platform.h"

#include <logging.h>

#include <QOpenGLFramebufferObject>
#include <private/qopenglcontext_p.h>

namespace KWin
{

namespace QPA
{

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context)
    : SharingPlatformContext(context, EGL_NO_SURFACE)
{
}

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context, const EGLSurface &surface, EGLConfig config)
    : AbstractPlatformContext(context, kwinApp()->platform()->sceneEglDisplay(), config)
    , m_surface(surface)
{
    create();
}

bool SharingPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    EGLSurface eglSurface;
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        eglSurface = m_surface;
    } else {
        eglSurface = static_cast<OffscreenSurface *>(surface)->nativeHandle();
    }

    const bool ok = eglMakeCurrent(eglDisplay(), eglSurface, eglSurface, eglContext());
    if (!ok) {
        qCWarning(KWIN_QPA, "eglMakeCurrent failed: %x", eglGetError());
        return false;
    }

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        // QOpenGLContextPrivate::setCurrentContext will be called after this
        // method returns, but that's too late, as we need a current context in
        // order to bind the content framebuffer object.
        QOpenGLContextPrivate::setCurrentContext(context());

        Window *window = static_cast<Window *>(surface);
        window->bindContentFBO();
    }

    return true;
}

bool SharingPlatformContext::isSharing() const
{
    return false;
}

void SharingPlatformContext::swapBuffers(QPlatformSurface *surface)
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        Window *window = static_cast<Window *>(surface);
        InternalClient *client = window->client();
        if (!client) {
            return;
        }
        context()->makeCurrent(surface->surface());
        glFlush();
        client->present(window->swapFBO());
        window->bindContentFBO();
    }
}

GLuint SharingPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (Window *window = dynamic_cast<Window*>(surface)) {
        const auto &fbo = window->contentFBO();
        if (!fbo.isNull()) {
            return fbo->handle();
        }
        qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
    }

    return 0;
}

void SharingPlatformContext::create()
{
    if (!config()) {
        qCWarning(KWIN_QPA) << "Did not get an EGL config";
        return;
    }
    if (!bindApi()) {
        qCWarning(KWIN_QPA) << "Could not bind API.";
        return;
    }
    createContext(kwinApp()->platform()->sceneEglContext());
}

}
}
