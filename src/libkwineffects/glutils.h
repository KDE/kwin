/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "libkwineffects/colorspace.h"
#include "libkwineffects/glframebuffer.h"
#include "libkwineffects/glshader.h"
#include "libkwineffects/glshadermanager.h"
#include "libkwineffects/gltexture.h"
#include "libkwineffects/glutils_funcs.h"
#include "libkwineffects/glvertexbuffer.h"

#include <QByteArray>
#include <QList>
#include <functional>

namespace KWin
{

// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
typedef void (*resolveFuncPtr)();
void KWIN_EXPORT initGL(const std::function<resolveFuncPtr(const char *)> &resolveFunction);
// Cleans up all resources hold by the GL Context
void KWIN_EXPORT cleanupGL();

bool KWIN_EXPORT hasGLVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool KWIN_EXPORT hasGLExtension(const QByteArray &extension);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool KWIN_EXPORT checkGLError(const char *txt);

QList<QByteArray> KWIN_EXPORT openGLExtensions();

} // namespace

/** @} */
