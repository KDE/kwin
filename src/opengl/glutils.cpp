/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/glutils.h"
#include "glplatform.h"
#include "gltexture_p.h"
#include "utils/common.h"

namespace KWin
{

static QString formatGLError(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return QStringLiteral("GL_NO_ERROR");
    case GL_INVALID_ENUM:
        return QStringLiteral("GL_INVALID_ENUM");
    case GL_INVALID_VALUE:
        return QStringLiteral("GL_INVALID_VALUE");
    case GL_INVALID_OPERATION:
        return QStringLiteral("GL_INVALID_OPERATION");
    case GL_STACK_OVERFLOW:
        return QStringLiteral("GL_STACK_OVERFLOW");
    case GL_STACK_UNDERFLOW:
        return QStringLiteral("GL_STACK_UNDERFLOW");
    case GL_OUT_OF_MEMORY:
        return QStringLiteral("GL_OUT_OF_MEMORY");
    default:
        return QLatin1String("0x") + QString::number(err, 16);
    }
}

bool checkGLError(const char *txt)
{
    GLenum err = glGetError();
    if (err == GL_CONTEXT_LOST) {
        qCWarning(KWIN_OPENGL) << "GL error: context lost";
        return true;
    }
    bool hasError = false;
    while (err != GL_NO_ERROR) {
        qCWarning(KWIN_OPENGL) << "GL error (" << txt << "): " << formatGLError(err);
        hasError = true;
        err = glGetError();
        if (err == GL_CONTEXT_LOST) {
            qCWarning(KWIN_OPENGL) << "GL error: context lost";
            break;
        }
    }
    return hasError;
}

} // namespace
