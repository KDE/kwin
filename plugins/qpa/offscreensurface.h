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

#pragma once

#include <epoxy/egl.h>
#include "fixqopengl.h"
#include <fixx11h.h>

#include <qpa/qplatformoffscreensurface.h>

namespace KWin
{
namespace QPA
{

class OffscreenSurface : public QPlatformOffscreenSurface
{
public:
    explicit OffscreenSurface(QOffscreenSurface *surface);
    ~OffscreenSurface() override;

    QSurfaceFormat format() const override;
    bool isValid() const override;

    EGLSurface nativeHandle() const;

private:
    QSurfaceFormat m_format;

    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
};

} // namespace QPA
} // namespace KWin
