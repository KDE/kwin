/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"

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
}

bool WindowScreenCastSource::isValid() const
{
    return !m_window.isNull();
}

bool WindowScreenCastSource::hasAlphaChannel() const
{
    return false;
}

QSize WindowScreenCastSource::textureSize() const
{
    return m_window->bufferGeometry().size();
}

void WindowScreenCastSource::render(GLRenderTarget *target)
{
    const QRect geometry = m_window->bufferGeometry();
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
