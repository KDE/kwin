/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "input.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include <drm_fourcc.h>

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
{
    m_timer.setInterval(0);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        Q_EMIT frame(QRegion(0, 0, m_window->width(), m_window->height()));
    });

    connect(m_window, &Window::closed, this, &ScreenCastSource::closed);
}

WindowScreenCastSource::~WindowScreenCastSource()
{
    pause();
}

quint32 WindowScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

QSize WindowScreenCastSource::textureSize() const
{
    return (m_window->clientGeometry().size() * m_window->targetScale()).toSize();
}

qreal WindowScreenCastSource::devicePixelRatio() const
{
    return m_window->targetScale();
}

void WindowScreenCastSource::render(QImage *target)
{
    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!offscreenTexture) {
        return;
    }
    offscreenTexture->setContentTransform(OutputTransform::FlipY);

    GLFramebuffer offscreenTarget(offscreenTexture.get());
    render(&offscreenTarget);
    grabTexture(offscreenTexture.get(), target);
}

void WindowScreenCastSource::render(GLFramebuffer *target)
{
    RenderTarget renderTarget(target);
    RenderViewport viewport(m_window->clientGeometry(), 1, renderTarget);

    Compositor::self()->scene()->renderer()->beginFrame(renderTarget, viewport);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    Compositor::self()->scene()->renderer()->renderItem(renderTarget, viewport, m_window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), WindowPaintData{}, {});
    Compositor::self()->scene()->renderer()->endFrame();
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_window->output()->renderLoop()->lastPresentationTimestamp();
}

uint WindowScreenCastSource::refreshRate() const
{
    return m_window->output()->refreshRate();
}

void WindowScreenCastSource::report()
{
    m_timer.start();
}

void WindowScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    if (m_window) {
        m_window->unrefOffscreenRendering();
        disconnect(m_window, &Window::damaged, this, &WindowScreenCastSource::report);
    }
    m_timer.stop();
    m_active = false;
}

void WindowScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    m_window->refOffscreenRendering();
    connect(m_window, &Window::damaged, this, &WindowScreenCastSource::report);
    m_timer.start();

    m_active = true;
}

bool WindowScreenCastSource::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }

    if (!m_window->clientGeometry().intersects(cursor->geometry())) {
        return false;
    }

    return input()->findToplevel(cursor->pos()) == m_window;
}

QPointF WindowScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return point - m_window->clientGeometry().topLeft();
}

QRectF WindowScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-m_window->clientGeometry().topLeft());
}

} // namespace KWin

#include "moc_windowscreencastsource.cpp"
