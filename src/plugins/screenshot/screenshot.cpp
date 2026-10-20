/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "screenshotdbusinterface2.h"

#include "compositor.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/renderdevice.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "opengl/eglbackend.h"
#include "opengl/glplatform.h"
#include "opengl/glutils.h"
#include "scene/decorationitem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "screenshotlayer.h"
#include "window.h"
#include "workspace.h"

#include <QPainter>

namespace KWin
{

static bool shouldFilterWindowFromCapture(Window *window, std::optional<pid_t> pidToHide)
{
    return window->excludeFromCapture() || (pidToHide.has_value() && window->pid() == *pidToHide);
}

std::optional<ScreenShotBuffer> ScreenShotBuffer::allocate(const QSize &size, qreal devicePixelRatio, QImage::Format format)
{
    const int bytesPerPixel = 4;
    const int bytesPerLine = size.width() * bytesPerPixel;
    const int sizeInBytes = size.height() * bytesPerLine;

    FileDescriptor fd = AnonymousFile::allocate("screenshot", sizeInBytes);
    if (!fd) {
        return std::nullopt;
    }

    MemoryMap memoryMap = MemoryMap(sizeInBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    if (!memoryMap.isValid()) {
        return std::nullopt;
    }

    return ScreenShotBuffer{
        .file = std::move(fd),
        .map = std::move(memoryMap),
        .size = size,
        .bytesPerLine = bytesPerLine,
        .format = format,
        .devicePixelRatio = devicePixelRatio,
    };
}

ScreenShotManager::ScreenShotManager()
    : m_dbusInterface2(new ScreenShotDBusInterface2(this))
{
}

ScreenShotManager::~ScreenShotManager()
{
}

// TODO share code with the screencast plugin?

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(LogicalOutput *screen, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    RenderDevice *device = Compositor::self()->primaryDevice();
    const auto context = device->eglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        scale = screen->scale();
    }
    const QSize nativeSize = (screen->geometryF().size() * scale).toSize();

    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }
    offscreenTexture->setContentTransform(OutputTransform::FlipY);
    offscreenTexture->setFilter(GL_LINEAR);
    offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    const auto target = std::make_unique<GLFramebuffer>(offscreenTexture.get());
    if (!target->valid()) {
        return std::nullopt;
    }

    ScreenshotLayer layer(screen, target.get());
    if (!layer.preparePresentationTest()) {
        return std::nullopt;
    }

    SceneView sceneView(kwinApp()->scene(), screen, nullptr, &layer, device);
    std::unique_ptr<ItemTreeView> cursorView;
    if (!(flags & ScreenShotIncludeCursor)) {
        cursorView = std::make_unique<ItemTreeView>(&sceneView, kwinApp()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
        cursorView->setExclusive(true);
    }
    sceneView.addWindowFilter([pidToHide](Window *window) {
        return shouldFilterWindowFromCapture(window, pidToHide);
    });
    const Rect fullDamage = Rect(QPoint(), target->size());
    sceneView.setViewport(screen->geometryF());
    sceneView.setScale(scale);

    sceneView.prePaint();
    const auto _ = qScopeGuard([&sceneView]() {
        sceneView.postPaint();
    });

    const auto beginInfo = layer.beginFrame();
    if (!beginInfo) {
        return std::nullopt;
    }
    sceneView.paint(beginInfo->renderTarget, QPoint(), fullDamage);
    if (!layer.endFrame(fullDamage, fullDamage, nullptr)) {
        return std::nullopt;
    }

    auto file = ScreenShotBuffer::allocate(offscreenTexture->size(), scale, QImage::Format_RGBX8888);
    if (!file) {
        return std::nullopt;
    }

    GLFramebuffer::pushFramebuffer(target.get());
    context->glReadnPixels(0, 0, offscreenTexture->width(), offscreenTexture->height(), GL_RGBA, GL_UNSIGNED_BYTE, file->map.size(), file->map.data());
    GLFramebuffer::popFramebuffer();

    return file;
}

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(const Rect &area, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    RenderDevice *device = Compositor::self()->primaryDevice();
    const auto context = device->eglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        const auto outputs = workspace()->outputs();
        for (LogicalOutput *output : outputs) {
            scale = std::max(scale, output->scale());
        }
    }
    const QSize nativeSize = area.size() * scale;

    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }
    offscreenTexture->setContentTransform(OutputTransform::FlipY);
    offscreenTexture->setFilter(GL_LINEAR);
    offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    const auto target = std::make_unique<GLFramebuffer>(offscreenTexture.get());
    if (!target->valid()) {
        return std::nullopt;
    }

    ScreenshotLayer layer(workspace()->outputs().front(), target.get());
    if (!layer.preparePresentationTest()) {
        return std::nullopt;
    }

    SceneView sceneView(kwinApp()->scene(), workspace()->outputs().front(), nullptr, &layer, device);
    std::unique_ptr<ItemTreeView> cursorView;
    if (!(flags & ScreenShotIncludeCursor)) {
        cursorView = std::make_unique<ItemTreeView>(&sceneView, kwinApp()->scene()->cursorItem(), workspace()->outputs().front(), nullptr, nullptr);
        cursorView->setExclusive(true);
    }
    sceneView.addWindowFilter([pidToHide](Window *window) {
        return shouldFilterWindowFromCapture(window, pidToHide);
    });
    const Rect fullDamage = Rect(QPoint(), target->size());
    sceneView.setViewport(area);
    sceneView.setScale(scale);

    sceneView.prePaint();
    const auto _ = qScopeGuard([&sceneView]() {
        sceneView.postPaint();
    });

    const auto beginInfo = layer.beginFrame();
    if (!beginInfo) {
        return std::nullopt;
    }
    sceneView.paint(beginInfo->renderTarget, QPoint(), fullDamage);
    if (!layer.endFrame(fullDamage, fullDamage, nullptr)) {
        return std::nullopt;
    }

    auto file = ScreenShotBuffer::allocate(offscreenTexture->size(), scale, QImage::Format_RGBX8888);
    if (!file) {
        return std::nullopt;
    }

    GLFramebuffer::pushFramebuffer(target.get());
    context->glReadnPixels(0, 0, offscreenTexture->width(), offscreenTexture->height(), GL_RGBA, GL_UNSIGNED_BYTE, file->map.size(), file->map.data());
    GLFramebuffer::popFramebuffer();

    return file;
}

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(Window *window, ScreenShotFlags flags)
{
    if (window->excludeFromCapture()) {
        return std::nullopt;
    }

    RenderDevice *device = Compositor::self()->primaryDevice();
    const auto context = device->eglContext();
    if (!context || !context->makeCurrent()) {
        return std::nullopt;
    }

    const qreal scale = window->targetScale();
    RectF geometry = window->visibleGeometry();
    if (window->windowItem()->decorationItem() && !(flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry();
    } else if (!(flags & ScreenShotIncludeShadow)) {
        geometry = window->frameGeometry();
    }
    const QSize nativeSize = (geometry.size() * scale).toSize();
    const auto offscreenTexture = GLTexture::allocate(GL_RGBA8, nativeSize);
    if (!offscreenTexture) {
        return std::nullopt;
    }
    offscreenTexture->setContentTransform(OutputTransform::FlipY);

    GLFramebuffer offscreenTarget(offscreenTexture.get());

    RenderTarget renderTarget(&offscreenTarget);
    RenderViewport viewport(geometry, scale, renderTarget, QPoint());

    WorkspaceScene *scene = kwinApp()->scene();
    auto renderer = scene->renderer(device);

    renderer->beginFrame(renderTarget, viewport);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    const bool windowRender = renderer->renderItem(renderTarget, viewport, window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, Region::infinite(), WindowPaintData{}, [flags, w = window->windowItem()](Item *item) {
        const bool deco = flags & ScreenShotFlag::ScreenShotIncludeDecoration;
        const bool shadow = deco && (flags & ScreenShotFlag::ScreenShotIncludeShadow);
        return (!deco && item == w->decorationItem())
            || (!shadow && item == w->shadowItem());
    }, {});
    if (!windowRender) {
        return std::nullopt;
    }
    if ((flags & ScreenShotFlag::ScreenShotIncludeCursor) && scene->cursorItem()->isVisible()) {
        if (!renderer->renderItem(renderTarget, viewport, scene->cursorItem(), 0, Region::infinite(), WindowPaintData{}, {}, {})) {
            return std::nullopt;
        }
    }
    renderer->endFrame();

    auto file = ScreenShotBuffer::allocate(offscreenTexture->size(), scale, QImage::Format_RGBA8888_Premultiplied);
    if (!file) {
        return std::nullopt;
    }

    GLFramebuffer::pushFramebuffer(&offscreenTarget);
    context->glReadnPixels(0, 0, offscreenTexture->width(), offscreenTexture->height(), GL_RGBA, GL_UNSIGNED_BYTE, file->map.size(), file->map.data());
    GLFramebuffer::popFramebuffer();

    return file;
}

} // namespace KWin

#include "moc_screenshot.cpp"
