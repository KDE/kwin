/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/backendoutput.h"
#include "core/renderloop.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "input.h"
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <drm_fourcc.h>

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window)
    : ScreenCastSource()
{
    add(window);

    connect(workspace(), &Workspace::windowAdded, this, [this](Window *window) {
        if (window->isPopupWindow() && m_windows.contains(window->transientFor())) {
            add(window);
            if (m_active) {
                Q_EMIT frame();
            }
        }
    });
}

WindowScreenCastSource::~WindowScreenCastSource()
{
    pause();
}

void WindowScreenCastSource::add(Window *window)
{
    m_windows.push_back(window);

    connect(window, &Window::closed, this, [this, window] {
        m_windows.removeOne(window);
        if (m_active) {
            unwatch(window);
            Q_EMIT frame();
        }
        if (m_windows.empty()) {
            Q_EMIT closed();
        }
    });

    if (m_active) {
        watch(window);
    }

    for (const auto child : window->transients()) {
        if (child->isPopupWindow()) {
            add(child);
        }
    }
}

void WindowScreenCastSource::watch(Window *window)
{
    window->refOffscreenRendering();
    connect(window, &Window::damaged, this, &WindowScreenCastSource::frame);
}

void WindowScreenCastSource::unwatch(Window *window)
{
    window->unrefOffscreenRendering();
    disconnect(window, &Window::damaged, this, &WindowScreenCastSource::frame);
}

quint32 WindowScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

QSize WindowScreenCastSource::textureSize() const
{
    return (boundingRect().size() * m_windows[0]->targetScale()).toSize();
}

qreal WindowScreenCastSource::devicePixelRatio() const
{
    return m_windows[0]->targetScale();
}

void WindowScreenCastSource::setRenderCursor(bool enable)
{
    m_renderCursor = enable;
}

QRegion WindowScreenCastSource::render(QImage *target, const QRegion &bufferDamage)
{
    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, target->size());
    if (!offscreenTexture) {
        return QRegion{};
    }
    offscreenTexture->setContentTransform(OutputTransform::FlipY);

    GLFramebuffer offscreenTarget(offscreenTexture.get());
    render(&offscreenTarget, infiniteRegion());
    grabTexture(offscreenTexture.get(), target);
    return QRect(QPoint(), target->size());
}

QRegion WindowScreenCastSource::render(GLFramebuffer *target, const QRegion &bufferDamage)
{
    RenderTarget renderTarget(target);
    RenderViewport viewport(boundingRect(), devicePixelRatio(), renderTarget, QPoint());

    WorkspaceScene *scene = Compositor::self()->scene();

    scene->renderer()->beginFrame(renderTarget, viewport);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    for (const auto &window : m_windows) {
        scene->renderer()->renderItem(renderTarget, viewport, window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), WindowPaintData{}, {}, {});
    }
    if (m_renderCursor && scene->cursorItem()->isVisible()) {
        scene->renderer()->renderItem(renderTarget, viewport, scene->cursorItem(), 0, infiniteRegion(), WindowPaintData{}, {}, {});
    }
    scene->renderer()->endFrame();
    return QRect(QPoint(), target->size());
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_windows[0]->output()->backendOutput()->renderLoop()->lastPresentationTimestamp();
}

uint WindowScreenCastSource::refreshRate() const
{
    return m_windows[0]->output()->refreshRate();
}

void WindowScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    for (const auto &window : std::as_const(m_windows)) {
        unwatch(window);
    }
    m_active = false;
}

void WindowScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    for (const auto &window : std::as_const(m_windows)) {
        watch(window);
    }
    Q_EMIT frame();

    m_active = true;
}

bool WindowScreenCastSource::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }

    if (!boundingRect().intersects(cursor->geometry())) {
        return false;
    }

    return m_windows.contains(input()->findToplevel(cursor->pos()));
}

QPointF WindowScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return point - boundingRect().topLeft();
}

QRectF WindowScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(-boundingRect().topLeft());
}

QRectF WindowScreenCastSource::boundingRect() const
{
    QRectF boundingRect;
    for (const auto &window : m_windows) {
        boundingRect = boundingRect.united(window->frameGeometry());
    }
    return boundingRect;
}

} // namespace KWin

#include "moc_windowscreencastsource.cpp"
