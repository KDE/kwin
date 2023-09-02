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
#include "libkwineffects/glvertexbuffer.h"
#include "libkwineffects/kwingltexture.h"
#include "libkwineffects/kwinglutils_export.h"
#include "libkwineffects/kwinglutils_funcs.h"

#include <QByteArray>
#include <QList>
#include <functional>

namespace KWin
{

// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
typedef void (*resolveFuncPtr)();
void KWINGLUTILS_EXPORT initGL(const std::function<resolveFuncPtr(const char *)> &resolveFunction);
// Cleans up all resources hold by the GL Context
void KWINGLUTILS_EXPORT cleanupGL();

bool KWINGLUTILS_EXPORT hasGLVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool KWINGLUTILS_EXPORT hasGLExtension(const QByteArray &extension);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool KWINGLUTILS_EXPORT checkGLError(const char *txt);

QList<QByteArray> KWINGLUTILS_EXPORT openGLExtensions();

} // namespace

/** @} */
