/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "compositor_wayland.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/renderlayerdelegate.h"
#include "cursor.h"
#include "ftrace.h"
#include "main.h"
#include "scene/surfaceitem.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

namespace KWin
{

WaylandCompositor *WaylandCompositor::create(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    auto *compositor = new WaylandCompositor(parent);
    s_compositor = compositor;
    return compositor;
}

WaylandCompositor::WaylandCompositor(QObject *parent)
    : Compositor(parent)
{
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed,
            this, &WaylandCompositor::destroyCompositorSelection);
}

WaylandCompositor::~WaylandCompositor()
{
    Q_EMIT aboutToDestroy();
    stop(); // this can't be called in the destructor of Compositor
}

void WaylandCompositor::toggleCompositing()
{
    // For the shortcut. Not possible on Wayland because we always composite.
}

void WaylandCompositor::start()
{
    if (!Compositor::setupStart()) {
        // Internal setup failed, abort.
        return;
    }

    if (Workspace::self()) {
        startupWithWorkspace();
    } else {
        connect(kwinApp(), &Application::workspaceCreated,
                this, &WaylandCompositor::startupWithWorkspace);
    }
}

void WaylandCompositor::framePass(RenderLayer *layer)
{
    layer->delegate()->frame();
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        framePass(sublayer);
    }
}

void WaylandCompositor::prePaintPass(RenderLayer *layer)
{
    layer->delegate()->prePaint();
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        prePaintPass(sublayer);
    }
}

void WaylandCompositor::postPaintPass(RenderLayer *layer)
{
    layer->delegate()->postPaint();
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        postPaintPass(sublayer);
    }
}

void WaylandCompositor::preparePaintPass(RenderLayer *layer, QRegion *repaint)
{
    // TODO: Cull opaque region.
    *repaint += layer->mapToGlobal(layer->repaints() + layer->delegate()->repaints());
    layer->resetRepaints();
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        if (sublayer->isVisible()) {
            preparePaintPass(sublayer, repaint);
        }
    }
}

void WaylandCompositor::paintPass(RenderLayer *layer, const RenderTarget &renderTarget, const QRegion &region)
{
    layer->delegate()->paint(renderTarget, region);

    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        if (sublayer->isVisible()) {
            paintPass(sublayer, renderTarget, region);
        }
    }
}

void WaylandCompositor::composite(RenderLoop *renderLoop)
{
    if (m_backend->checkGraphicsReset()) {
        qCDebug(KWIN_CORE) << "Graphics reset occurred";
#if KWIN_BUILD_NOTIFICATIONS
        KNotification::event(QStringLiteral("graphicsreset"), i18n("Desktop effects were restarted due to a graphics reset"));
#endif
        reinitialize();
        return;
    }

    Output *output = findOutput(renderLoop);
    OutputLayer *primaryLayer = m_backend->primaryLayer(output);
    fTraceDuration("Paint (", output->name(), ")");

    RenderLayer *superLayer = m_superlayers[renderLoop];
    superLayer->setOutputLayer(primaryLayer);

    renderLoop->prepareNewFrame();

    if (superLayer->needsRepaint()) {
        renderLoop->beginPaint();
        prePaintPass(superLayer);

        SurfaceItem *scanoutCandidate = superLayer->delegate()->scanoutCandidate();
        renderLoop->setFullscreenSurface(scanoutCandidate);
        output->setContentType(scanoutCandidate ? scanoutCandidate->contentType() : ContentType::None);

        bool directScanout = false;
        if (scanoutCandidate) {
            const auto sublayers = superLayer->sublayers();
            const bool scanoutPossible = std::none_of(sublayers.begin(), sublayers.end(), [](RenderLayer *sublayer) {
                return sublayer->isVisible();
            });
            if (scanoutPossible && !output->directScanoutInhibited()) {
                directScanout = primaryLayer->scanout(scanoutCandidate);
            }
        }

        if (!directScanout) {
            QRegion surfaceDamage = primaryLayer->repaints();
            primaryLayer->resetRepaints();
            preparePaintPass(superLayer, &surfaceDamage);

            if (auto beginInfo = primaryLayer->beginFrame()) {
                auto &[renderTarget, repaint] = beginInfo.value();

                const QRegion bufferDamage = surfaceDamage.united(repaint).intersected(superLayer->rect().toAlignedRect());

                paintPass(superLayer, renderTarget, bufferDamage);
                primaryLayer->endFrame(bufferDamage, surfaceDamage);
            }
        }

        postPaintPass(superLayer);
    }

    m_backend->present(output);

    framePass(superLayer);

    // TODO: Put it inside the cursor layer once the cursor layer can be backed by a real output layer.
    if (waylandServer()) {
        const std::chrono::milliseconds frameTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(output->renderLoop()->lastPresentationTimestamp());

        if (!Cursors::self()->isCursorHidden()) {
            Cursor *cursor = Cursors::self()->currentCursor();
            if (cursor->geometry().intersects(output->geometry())) {
                cursor->markAsRendered(frameTime);
            }
        }
    }
}

} // namespace KWin

#include "moc_compositor_wayland.cpp"
