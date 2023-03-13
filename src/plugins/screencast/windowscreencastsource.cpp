/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "composite.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "effects.h"
#include "libkwineffects/kwineffects.h"
#include "libkwineffects/kwingltexture.h"
#include "libkwineffects/kwinglutils.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include <drm_fourcc.h>

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
    , m_offscreenRef(window)
{
    connect(m_window, &Window::closed, this, &ScreenCastSource::closed);
}

quint32 WindowScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_RGBA8888;
}

bool WindowScreenCastSource::hasAlphaChannel() const
{
    return true;
}

QSize WindowScreenCastSource::textureSize() const
{
    return m_window->clientGeometry().size().toSize();
}

void WindowScreenCastSource::render(spa_data *spa, spa_video_format format)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLFramebuffer offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, spa, format);
}

void WindowScreenCastSource::render(GLFramebuffer *target)
{
    const QRectF geometry = m_window->clientGeometry();
    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(geometry);

    WindowPaintData data;
    data.setProjectionMatrix(projectionMatrix);

    RenderTarget renderTarget(target);
    RenderViewport viewport(geometry, 1, renderTarget);
    GLFramebuffer::pushFramebuffer(target);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    Compositor::self()->scene()->renderer()->renderItem(renderTarget, viewport, m_window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), data);
    GLFramebuffer::popFramebuffer();
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_window->output()->renderLoop()->lastPresentationTimestamp();
}

} // namespace KWin
