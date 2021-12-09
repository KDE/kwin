/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "deleted.h"
#include "effects.h"
#include "kwineffects.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "scene.h"
#include "toplevel.h"

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Toplevel *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
{
    connect(m_window, &Toplevel::windowClosed, this, &ScreenCastSource::closed);
}

bool WindowScreenCastSource::hasAlphaChannel() const
{
    return true;
}

QSize WindowScreenCastSource::textureSize() const
{
    return m_window->clientGeometry().size();
}

void WindowScreenCastSource::render(QImage *image)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLRenderTarget offscreenTarget(offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, image);
}

void WindowScreenCastSource::render(GLRenderTarget *target)
{
    const QRect geometry = m_window->clientGeometry();
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(geometry.x(), geometry.x() + geometry.width(),
                           geometry.y(), geometry.y() + geometry.height(), -1, 1);

    EffectWindowImpl *effectWindow = m_window->effectWindow();
    WindowPaintData data(effectWindow);
    data.setProjectionMatrix(projectionMatrix);

    GLRenderTarget::pushRenderTarget(target);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    effectWindow->sceneWindow()->performPaint(Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), data);
    GLRenderTarget::popRenderTarget();
}

} // namespace KWin
