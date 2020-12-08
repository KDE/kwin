/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglhelpers.h"

#include <logging.h>

#include <QOpenGLContext>

namespace KWin
{
namespace QPA
{

bool isOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }

    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

EGLConfig configFromFormat(EGLDisplay display, const QSurfaceFormat &surfaceFormat, EGLint surfaceType)
{
    // qMax as these values are initialized to -1 by default.
    const EGLint redSize = qMax(surfaceFormat.redBufferSize(), 0);
    const EGLint greenSize = qMax(surfaceFormat.greenBufferSize(), 0);
    const EGLint blueSize = qMax(surfaceFormat.blueBufferSize(), 0);
    const EGLint alphaSize = qMax(surfaceFormat.alphaBufferSize(), 0);
    const EGLint depthSize = qMax(surfaceFormat.depthBufferSize(), 0);
    const EGLint stencilSize = qMax(surfaceFormat.stencilBufferSize(), 0);

    const EGLint renderableType = isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT;

    // Not setting samples as QtQuick doesn't need it.
    const QVector<EGLint> attributes {
        EGL_SURFACE_TYPE, surfaceType,
        EGL_RED_SIZE, redSize,
        EGL_GREEN_SIZE, greenSize,
        EGL_BLUE_SIZE, blueSize,
        EGL_ALPHA_SIZE, alphaSize,
        EGL_DEPTH_SIZE, depthSize,
        EGL_STENCIL_SIZE, stencilSize,
        EGL_RENDERABLE_TYPE, renderableType,
        EGL_NONE
    };

    EGLint configCount;
    EGLConfig configs[1024];
    if (!eglChooseConfig(display, attributes.data(), configs, 1, &configCount)) {
        // FIXME: Don't bail out yet, we should try to find the most suitable config.
        qCWarning(KWIN_QPA, "eglChooseConfig failed: %x", eglGetError());
        return EGL_NO_CONFIG_KHR;
    }

    if (configCount != 1) {
        qCWarning(KWIN_QPA) << "eglChooseConfig did not return any configs";
        return EGL_NO_CONFIG_KHR;
    }

    return configs[0];
}

QSurfaceFormat formatFromConfig(EGLDisplay display, EGLConfig config)
{
    int redSize = 0;
    int blueSize = 0;
    int greenSize = 0;
    int alphaSize = 0;
    int stencilSize = 0;
    int depthSize = 0;
    int sampleCount = 0;

    eglGetConfigAttrib(display, config, EGL_RED_SIZE, &redSize);
    eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &greenSize);
    eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blueSize);
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alphaSize);
    eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE, &stencilSize);
    eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depthSize);
    eglGetConfigAttrib(display, config, EGL_SAMPLES, &sampleCount);

    QSurfaceFormat format;
    format.setRedBufferSize(redSize);
    format.setGreenBufferSize(greenSize);
    format.setBlueBufferSize(blueSize);
    format.setAlphaBufferSize(alphaSize);
    format.setStencilBufferSize(stencilSize);
    format.setDepthBufferSize(depthSize);
    format.setSamples(sampleCount);
    format.setRenderableType(isOpenGLES() ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    format.setStereo(false);

    return format;
}

} // namespace QPA
} // namespace KWin
