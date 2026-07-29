/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"
#include "anonymousfile.h"
#include "screenshotdbusinterface2.h"
#include "screenshotdbusinterface3.h"

#include "compositor.h"
#include "core/gpumanager.h"
#include "core/graphicsbufferallocator.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/renderbackend.h"
#include "core/renderdevice.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
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

std::optional<ScreenShotBuffer> ScreenShotBuffer::from(RenderDevice *device, GraphicsBuffer *buffer, FileDescriptor &&sync, qreal devicePixelRatio)
{
    const QSize size = buffer->size();
    const int bytesPerPixel = 4;
    const int bytesPerLine = size.width() * bytesPerPixel;
    const int sizeInBytes = size.height() * bytesPerLine;

    const auto info = FormatInfo::get(buffer->format());
    if (!info || !info->qimageFormat) {
        return std::nullopt;
    }

    FileDescriptor fd = AnonymousFile::allocate("screenshot", sizeInBytes);
    if (!fd) {
        return std::nullopt;
    }

    MemoryMap memoryMap = MemoryMap(sizeInBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    if (!memoryMap.isValid()) {
        return std::nullopt;
    }

    QImage target(static_cast<uchar *>(memoryMap.data()), size.width(), size.height(), bytesPerLine, *info->qimageFormat);
    device->download(buffer, std::move(sync), target);

    return ScreenShotBuffer{
        .file = std::move(fd),
        .map = std::move(memoryMap),
        .size = size,
        .bytesPerLine = bytesPerLine,
        .format = *info->qimageFormat,
        .devicePixelRatio = devicePixelRatio,
    };
}

ScreenShotManager::ScreenShotManager()
    : m_dbusInterface2(new ScreenShotDBusInterface2(this))
    , m_dbusInterface3(new ScreenShotDBusInterface3(this))
{
}

ScreenShotManager::~ScreenShotManager()
{
}

// TODO share code with the screencast plugin?

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(LogicalOutput *screen, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    RenderDevice *device = Compositor::self()->primaryDevice();
    RenderBackend *backend = Compositor::self()->backend();

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        scale = screen->scale();
    }
    const QSize nativeSize = (screen->geometryF().size() * scale).toSize();

    GraphicsBuffer *buffer = device->allocator()->allocate(GraphicsBufferOptions{
        .size = nativeSize,
        .format = DRM_FORMAT_XRGB8888,
        .modifiers = backend->supportedFormats(device)[DRM_FORMAT_XRGB8888],
        .software = false,
        .scanout = false,
        .render = true,
    });
    if (!buffer) {
        return std::nullopt;
    }
    auto dropBuffer = qScopeGuard([buffer]() {
        buffer->drop();
    });

    ScreenshotLayer layer(screen, buffer);
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
    const Rect fullDamage = Rect(QPoint(), buffer->size());
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
    FileDescriptor sync = sceneView.paint(*beginInfo, QPoint(), fullDamage, nullptr);
    if (!layer.endFrame(fullDamage, sync.duplicate(), nullptr)) {
        return std::nullopt;
    }

    return ScreenShotBuffer::from(device, buffer, std::move(sync), scale);
}

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(const Rect &area, ScreenShotFlags flags, std::optional<pid_t> pidToHide)
{
    RenderDevice *device = Compositor::self()->primaryDevice();
    RenderBackend *backend = Compositor::self()->backend();

    qreal scale = 1.0;
    if (flags & ScreenShotNativeResolution) {
        const auto outputs = workspace()->outputs();
        for (LogicalOutput *output : outputs) {
            scale = std::max(scale, output->scale());
        }
    }
    const QSize nativeSize = area.size() * scale;

    GraphicsBuffer *buffer = device->allocator()->allocate(GraphicsBufferOptions{
        .size = nativeSize,
        .format = DRM_FORMAT_XRGB8888,
        .modifiers = backend->supportedFormats(device)[DRM_FORMAT_XRGB8888],
        .software = false,
        .scanout = false,
        .render = true,
    });
    if (!buffer) {
        return std::nullopt;
    }
    auto dropBuffer = qScopeGuard([buffer]() {
        buffer->drop();
    });

    ScreenshotLayer layer(workspace()->outputs().front(), buffer);
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
    const Rect fullDamage = Rect(QPoint(), buffer->size());
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
    FileDescriptor sync = sceneView.paint(*beginInfo, QPoint(), fullDamage, nullptr);
    if (!layer.endFrame(fullDamage, sync.duplicate(), nullptr)) {
        return std::nullopt;
    }

    return ScreenShotBuffer::from(device, buffer, std::move(sync), scale);
}

std::optional<ScreenShotBuffer> ScreenShotManager::takeScreenShot(Window *window, ScreenShotFlags flags)
{
    if (window->excludeFromCapture()) {
        return std::nullopt;
    }

    RenderDevice *device = Compositor::self()->primaryDevice();
    RenderBackend *backend = Compositor::self()->backend();

    const qreal scale = window->targetScale();
    RectF geometry = window->visibleGeometry();
    if (window->windowItem()->decorationItem() && !(flags & ScreenShotIncludeDecoration)) {
        geometry = window->clientGeometry();
    } else if (!(flags & ScreenShotIncludeShadow)) {
        geometry = window->frameGeometry();
    }
    const QSize nativeSize = (geometry.size() * scale).toSize();

    GraphicsBuffer *buffer = device->allocator()->allocate(GraphicsBufferOptions{
        .size = nativeSize,
        .format = DRM_FORMAT_ARGB8888,
        .modifiers = backend->supportedFormats(device)[DRM_FORMAT_ARGB8888],
        .software = false,
        .scanout = false,
        .render = true,
    });
    if (!buffer) {
        return std::nullopt;
    }
    auto dropBuffer = qScopeGuard([buffer]() {
        buffer->drop();
    });

    WorkspaceScene *scene = kwinApp()->scene();
    auto renderer = scene->renderer(device);

    OutputLayerBeginFrameInfo beginInfo{
        .renderTarget = RenderTarget(buffer, OutputTransform::Normal),
        .repaint = Region::infinite(),
    };
    const auto renderInfo = renderer->beginFrame(beginInfo, nullptr, nullptr);
    if (!renderInfo) {
        return std::nullopt;
    }
    RenderViewport viewport(geometry, scale, renderInfo->renderTarget, QPoint());

    renderer->renderBackground(renderInfo->renderTarget, viewport, Region::infinite());
    const bool windowRender = renderer->renderItem(renderInfo->renderTarget, viewport, window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, Region::infinite(), WindowPaintData{}, [flags, w = window->windowItem()](Item *item) {
        const bool deco = flags & ScreenShotFlag::ScreenShotIncludeDecoration;
        const bool shadow = deco && (flags & ScreenShotFlag::ScreenShotIncludeShadow);
        return (!deco && item == w->decorationItem())
            || (!shadow && item == w->shadowItem());
    }, {});
    if (!windowRender) {
        return std::nullopt;
    }
    if ((flags & ScreenShotFlag::ScreenShotIncludeCursor) && scene->cursorItem()->isVisible()) {
        if (!renderer->renderItem(renderInfo->renderTarget, viewport, scene->cursorItem(), 0, Region::infinite(), WindowPaintData{}, {}, {})) {
            return std::nullopt;
        }
    }
    FileDescriptor sync = renderer->endFrame(Region::infinite(), nullptr, nullptr);

    return ScreenShotBuffer::from(device, buffer, std::move(sync), scale);
}

} // namespace KWin

#include "moc_screenshot.cpp"
