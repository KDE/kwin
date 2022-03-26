/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <epoxy/egl.h>

static QString getEglErrorString(EGLint errorCode)
{
    switch (errorCode) {
    case EGL_SUCCESS:
        return QStringLiteral("EGL_SUCCESS");
    case EGL_NOT_INITIALIZED:
        return QStringLiteral("EGL_NOT_INITIALIZED");
    case EGL_BAD_ACCESS:
        return QStringLiteral("EGL_BAD_ACCESS");
    case EGL_BAD_ALLOC:
        return QStringLiteral("EGL_BAD_ALLOC");
    case EGL_BAD_ATTRIBUTE:
        return QStringLiteral("EGL_BAD_ATTRIBUTE");
    case EGL_BAD_CONTEXT:
        return QStringLiteral("EGL_BAD_CONTEXT");
    case EGL_BAD_CONFIG:
        return QStringLiteral("EGL_BAD_CONFIG");
    case EGL_BAD_CURRENT_SURFACE:
        return QStringLiteral("EGL_BAD_CURRENT_SURFACE");
    case EGL_BAD_DISPLAY:
        return QStringLiteral("EGL_BAD_DISPLAY");
    case EGL_BAD_SURFACE:
        return QStringLiteral("EGL_BAD_SURFACE");
    case EGL_BAD_MATCH:
        return QStringLiteral("EGL_BAD_MATCH");
    case EGL_BAD_PARAMETER:
        return QStringLiteral("EGL_BAD_PARAMETER");
    case EGL_BAD_NATIVE_PIXMAP:
        return QStringLiteral("EGL_BAD_NATIVE_PIXMAP");
    case EGL_BAD_NATIVE_WINDOW:
        return QStringLiteral("EGL_BAD_NATIVE_WINDOW");
    case EGL_CONTEXT_LOST:
        return QStringLiteral("EGL_CONTEXT_LOST");
    default:
        return QString::number(errorCode, 16);
    }
}

static QString getEglErrorString()
{
    return getEglErrorString(eglGetError());
}
