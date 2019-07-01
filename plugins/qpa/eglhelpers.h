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

#define MESA_EGL_NO_X11_HEADERS
#include <epoxy/egl.h>
#include "fixqopengl.h"

#include <QSurfaceFormat>

namespace KWin
{
namespace QPA
{

bool isOpenGLES();

EGLConfig configFromFormat(EGLDisplay display, const QSurfaceFormat &surfaceFormat, EGLint surfaceType = 0);
QSurfaceFormat formatFromConfig(EGLDisplay display, EGLConfig config);

} // namespace QPA
} // namespace KWin
