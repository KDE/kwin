/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"

#include "compositor.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "input.h"
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
    Q_ASSERT(!m_windows.contains(window));
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
        if (child->readyForPainting() && child->isPopupWindow()) {
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

void WindowScreenCastSource::setNextPresentationTimestamp(std::chrono::nanoseconds timestamp, uint32_t refreshRate)
{
    // TODO use the timestamp for rendering, once we have a SceneView to do that with
}

std::optional<ScreenCastSource::RenderResult> WindowScreenCastSource::render(GraphicsBuffer *buffer, const Region &bufferDamage)
{
    WorkspaceScene *scene = kwinApp()->scene();
    auto renderer = scene->renderer(m_renderDevice);
    const Rect damage(QPoint(), buffer->size());

    OutputLayerBeginFrameInfo beginInfo{
        .renderTarget = RenderTarget{buffer, OutputTransform::FlipY},
        .repaint = damage,
    };
    const auto renderInfo = renderer->beginFrame(beginInfo, nullptr, nullptr);
    if (!renderInfo) {
        return std::nullopt;
    }
    RenderViewport viewport(boundingRect(), devicePixelRatio(), renderInfo->renderTarget, QPoint());

    renderer->renderBackground(renderInfo->renderTarget, viewport, Region::infinite());
    for (const auto &window : m_windows) {
        if (!renderer->renderItem(renderInfo->renderTarget, viewport, window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, Region::infinite(), WindowPaintData{}, {}, {})) {
            return std::nullopt;
        }
    }
    if (m_renderCursor && scene->cursorItem()->isVisible()) {
        if (!renderer->renderItem(renderInfo->renderTarget, viewport, scene->cursorItem(), 0, Region::infinite(), WindowPaintData{}, {}, {})) {
            return std::nullopt;
        }
    }
    return RenderResult{damage, renderer->endFrame(damage, nullptr, nullptr)};
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

RectF WindowScreenCastSource::mapFromGlobal(const RectF &rect) const
{
    return rect.translated(-boundingRect().topLeft());
}

RectF WindowScreenCastSource::boundingRect() const
{
    RectF boundingRect;
    for (const auto &window : m_windows) {
        boundingRect = boundingRect.united(window->frameGeometry());
    }
    return boundingRect;
}

} // namespace KWin

#include "moc_windowscreencastsource.cpp"
