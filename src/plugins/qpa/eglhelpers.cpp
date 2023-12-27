/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eglhelpers.h"
#include "opengl/egldisplay.h"

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

EGLConfig configFromFormat(EglDisplay *display, const QSurfaceFormat &surfaceFormat, EGLint surfaceType)
{
    // std::max as these values are initialized to -1 by default.
    const EGLint redSize = std::max(surfaceFormat.redBufferSize(), 0);
    const EGLint greenSize = std::max(surfaceFormat.greenBufferSize(), 0);
    const EGLint blueSize = std::max(surfaceFormat.blueBufferSize(), 0);
    const EGLint alphaSize = std::max(surfaceFormat.alphaBufferSize(), 0);
    const EGLint depthSize = std::max(surfaceFormat.depthBufferSize(), 0);
    const EGLint stencilSize = std::max(surfaceFormat.stencilBufferSize(), 0);

    const EGLint renderableType = isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT;

    // Not setting samples as QtQuick doesn't need it.
    const QList<EGLint> attributes{
        EGL_SURFACE_TYPE, surfaceType,
        EGL_RED_SIZE, redSize,
        EGL_GREEN_SIZE, greenSize,
        EGL_BLUE_SIZE, blueSize,
        EGL_ALPHA_SIZE, alphaSize,
        EGL_DEPTH_SIZE, depthSize,
        EGL_STENCIL_SIZE, stencilSize,
        EGL_RENDERABLE_TYPE, renderableType,
        EGL_NONE};

    EGLint configCount;
    if (!eglChooseConfig(display->handle(), attributes.data(), nullptr, 0, &configCount)) {
        qCWarning(KWIN_QPA, "eglChooseConfig failed: %x", eglGetError());
        return EGL_NO_CONFIG_KHR;
    }
    if (configCount == 0) {
        qCWarning(KWIN_QPA, "eglChooseConfig did not return any configs");
        return EGL_NO_CONFIG_KHR;
    }

    QList<EGLConfig> configs(configCount);
    if (!eglChooseConfig(display->handle(), attributes.data(), configs.data(), configCount, &configCount)) {
        qCWarning(KWIN_QPA, "eglChooseConfig failed: %x", eglGetError());
        return EGL_NO_CONFIG_KHR;
    }
    if (configCount != configs.size()) {
        qCWarning(KWIN_QPA, "eglChooseConfig did not return requested configs");
        return EGL_NO_CONFIG_KHR;
    }

    for (const EGLConfig &config : std::as_const(configs)) {
        EGLint redConfig, greenConfig, blueConfig, alphaConfig;
        eglGetConfigAttrib(display->handle(), config, EGL_RED_SIZE, &redConfig);
        eglGetConfigAttrib(display->handle(), config, EGL_GREEN_SIZE, &greenConfig);
        eglGetConfigAttrib(display->handle(), config, EGL_BLUE_SIZE, &blueConfig);
        eglGetConfigAttrib(display->handle(), config, EGL_ALPHA_SIZE, &alphaConfig);

        if ((redSize == 0 || redSize == redConfig) && (greenSize == 0 || greenSize == greenConfig) && (blueSize == 0 || blueSize == blueConfig) && (alphaSize == 0 || alphaSize == alphaConfig)) {
            return config;
        }
    }

    // Return first config as a fallback.
    return configs[0];
}

QSurfaceFormat formatFromConfig(EglDisplay *display, EGLConfig config)
{
    int redSize = 0;
    int blueSize = 0;
    int greenSize = 0;
    int alphaSize = 0;
    int stencilSize = 0;
    int depthSize = 0;
    int sampleCount = 0;

    eglGetConfigAttrib(display->handle(), config, EGL_RED_SIZE, &redSize);
    eglGetConfigAttrib(display->handle(), config, EGL_GREEN_SIZE, &greenSize);
    eglGetConfigAttrib(display->handle(), config, EGL_BLUE_SIZE, &blueSize);
    eglGetConfigAttrib(display->handle(), config, EGL_ALPHA_SIZE, &alphaSize);
    eglGetConfigAttrib(display->handle(), config, EGL_STENCIL_SIZE, &stencilSize);
    eglGetConfigAttrib(display->handle(), config, EGL_DEPTH_SIZE, &depthSize);
    eglGetConfigAttrib(display->handle(), config, EGL_SAMPLES, &sampleCount);

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
