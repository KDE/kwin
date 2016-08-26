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
#include "platformcontextwayland.h"
#include "integration.h"
#include "window.h"

namespace KWin
{

namespace QPA
{

PlatformContextWayland::PlatformContextWayland(QOpenGLContext *context, Integration *integration)
    : AbstractPlatformContext(context, integration, integration->eglDisplay())
{
    create();
}

bool PlatformContextWayland::makeCurrent(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    EGLSurface s = window->eglSurface();
    if (s == EGL_NO_SURFACE) {
        window->createEglSurface(eglDisplay(), config());
        s = window->eglSurface();
        if (s == EGL_NO_SURFACE) {
            return false;
        }
    }
    return eglMakeCurrent(eglDisplay(), s, s, eglContext());
}

bool PlatformContextWayland::isSharing() const
{
    return false;
}

void PlatformContextWayland::swapBuffers(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    EGLSurface s = window->eglSurface();
    if (s == EGL_NO_SURFACE) {
        return;
    }
    eglSwapBuffers(eglDisplay(), s);
}

void PlatformContextWayland::create()
{
    if (config() == 0) {
        return;
    }
    if (!bindApi()) {
        return;
    }
    createContext();
}

}
}
