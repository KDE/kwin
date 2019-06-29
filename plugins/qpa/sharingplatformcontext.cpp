/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "sharingplatformcontext.h"
#include "offscreensurface.h"
#include "window.h"
#include "../../platform.h"
#include "../../shell_client.h"
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
        auto c = window->shellClient();
        if (!c) {
            qCDebug(KWIN_QPA) << "SwapBuffers called but there is no ShellClient";
            return;
        }
        context()->makeCurrent(surface->surface());
        glFlush();
        c->setInternalFramebufferObject(window->swapFBO());
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
    if (config() == 0) {
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
