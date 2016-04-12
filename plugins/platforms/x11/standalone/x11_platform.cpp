/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "x11_platform.h"
#include <kwinconfig.h>
#if HAVE_EPOXY_GLX
#include "glxbackend.h"
#endif
#include "eglonxbackend.h"
#include "screens_xrandr.h"
#include "options.h"

#include <QX11Info>

namespace KWin
{

X11StandalonePlatform::X11StandalonePlatform(QObject *parent)
    : Platform(parent)
{
}

X11StandalonePlatform::~X11StandalonePlatform() = default;

void X11StandalonePlatform::init()
{
    if (!QX11Info::isPlatformX11()) {
        emit initFailed();
        return;
    }
    setReady(true);
    emit screensQueried();
}

Screens *X11StandalonePlatform::createScreens(QObject *parent)
{
    return new XRandRScreens(parent);
}

OpenGLBackend *X11StandalonePlatform::createOpenGLBackend()
{
    switch (options->glPlatformInterface()) {
#if HAVE_EPOXY_GLX
    case GlxPlatformInterface:
        return new GlxBackend();
#endif
    case EglPlatformInterface:
        return new EglOnXBackend();
    default:
        // no backend available
        return nullptr;
    }
}

}
