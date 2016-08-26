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
#include "integration.h"
#include "window.h"
#include "../../platform.h"
#include "../../wayland_server.h"
#include "../../shell_client.h"
#include <logging.h>

#include <QOpenGLFramebufferObject>

namespace KWin
{

namespace QPA
{

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context, Integration *integration)
    : SharingPlatformContext(context, integration, EGL_NO_SURFACE)
{
}

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context, Integration *integration, const EGLSurface &surface, EGLConfig config)
    : AbstractPlatformContext(context, integration, kwinApp()->platform()->sceneEglDisplay(), config)
    , m_surface(surface)
{
    create();
}

bool SharingPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    if (eglMakeCurrent(eglDisplay(), m_surface, m_surface, eglContext())) {
        window->bindContentFBO();
        return true;
    }
    qCWarning(KWIN_QPA) << "Failed to make context current";
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_QPA) << "EGL error code: " << error;
    }

    return false;
}

bool SharingPlatformContext::isSharing() const
{
    return false;
}

void SharingPlatformContext::swapBuffers(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
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

GLuint SharingPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (Window *window = dynamic_cast<Window*>(surface)) {
        const auto &fbo = window->contentFBO();
        if (!fbo.isNull()) {
            return fbo->handle();
        }
    }
    qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
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
