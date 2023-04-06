/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "composite.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "deleted.h"
#include "effects.h"
#include "kwineffects.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
    , m_offscreenRef(window)
{
    connect(m_window, &Window::windowClosed, this, &ScreenCastSource::closed);
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
    projectionMatrix.ortho(geometry.x(), geometry.x() + geometry.width(),
                           geometry.y(), geometry.y() + geometry.height(), -1, 1);

    WindowPaintData data;
    data.setProjectionMatrix(projectionMatrix);
    data.setRenderTargetScale(1.0);

    GLFramebuffer::pushFramebuffer(target);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    Compositor::self()->scene()->renderer()->renderItem(m_window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), data);
    GLFramebuffer::popFramebuffer();
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_window->output()->renderLoop()->lastPresentationTimestamp();
}

uint WindowScreenCastSource::refreshRate() const
{
    return m_window->output()->refreshRate();
}

} // namespace KWin
