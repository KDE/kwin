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
#include "scene.h"
#include "window.h"
#include "windowitem.h"

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
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

void WindowScreenCastSource::render(QImage *image)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLFramebuffer offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, image);
}

void WindowScreenCastSource::render(GLFramebuffer *target)
{
    auto targetScale = Compositor::self()->scene()->renderTargetScale();

    const QRectF geometry = scaledRect(m_window->clientGeometry(), targetScale);
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(geometry.x(), geometry.x() + geometry.width(),
                           geometry.y(), geometry.y() + geometry.height(), -1, 1);

    WindowPaintData data;
    data.setProjectionMatrix(projectionMatrix);

    GLFramebuffer::pushFramebuffer(target);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    Compositor::self()->scene()->render(m_window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), data);
    GLFramebuffer::popFramebuffer();
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_window->output()->renderLoop()->lastPresentationTimestamp();
}

} // namespace KWin
