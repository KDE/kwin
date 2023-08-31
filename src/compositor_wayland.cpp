/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "compositor_wayland.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/renderlayerdelegate.h"
#include "cursor.h"
#include "ftrace.h"
#include "main.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "scene/cursordelegate_opengl.h"
#include "scene/cursordelegate_qpainter.h"
#include "scene/cursorscene.h"
#include "scene/surfaceitem.h"
#include "scene/workspacescene.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

#include <QQuickWindow>

#include <xcb/composite.h>

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
    if (kwinApp()->isTerminating()) {
        // Don't start while KWin is terminating. An event to restart might be lingering
        // in the event queue due to graphics reset.
        return;
    }

    if (m_state != State::Off) {
        return;
    }
    m_state = State::Starting;

    options->reloadCompositingSettings(true);

    initializeX11();

    Q_EMIT aboutToToggleCompositing();

    const QVector<CompositingType> availableCompositors = kwinApp()->outputBackend()->supportedCompositors();
    QVector<CompositingType> candidateCompositors;

    // If compositing has been restarted, try to use the last used compositing type.
    if (m_selectedCompositor != NoCompositing) {
        candidateCompositors.append(m_selectedCompositor);
    } else {
        candidateCompositors = availableCompositors;

        const auto userConfigIt = std::find(candidateCompositors.begin(), candidateCompositors.end(), options->compositingMode());
        if (userConfigIt != candidateCompositors.end()) {
            candidateCompositors.erase(userConfigIt);
            candidateCompositors.prepend(options->compositingMode());
        } else {
            qCWarning(KWIN_CORE) << "Configured compositor not supported by Platform. Falling back to defaults";
        }
    }

    for (auto type : std::as_const(candidateCompositors)) {
        bool stop = false;
        switch (type) {
        case OpenGLCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the OpenGL scene";
            stop = attemptOpenGLCompositing();
            break;
        case QPainterCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the QPainter scene";
            stop = attemptQPainterCompositing();
            break;
        default:
            break;
        }

        if (stop) {
            break;
        } else if (qEnvironmentVariableIsSet("KWIN_COMPOSE")) {
            qCCritical(KWIN_CORE) << "Could not fulfill the requested compositing mode in KWIN_COMPOSE:" << type << ". Exiting.";
            qApp->quit();
        }
    }

    if (!m_backend) {
        m_state = State::Off;

        if (auto *con = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(con, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        }
        cleanupX11();
        return;
    }

    m_selectedCompositor = m_backend->compositingType();

    if (!Workspace::self() && m_backend && m_backend->compositingType() == QPainterCompositing) {
        // Force Software QtQuick on first startup with QPainter.
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    Q_EMIT sceneCreated();

    if (Workspace::self()) {
        startupWithWorkspace();
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, &WaylandCompositor::startupWithWorkspace);
    }
}

void WaylandCompositor::startupWithWorkspace()
{
    Q_ASSERT(m_scene);
    m_scene->initialize();
    m_cursorScene->initialize();

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }
    connect(workspace(), &Workspace::outputAdded, this, &WaylandCompositor::addOutput);
    connect(workspace(), &Workspace::outputRemoved, this, &WaylandCompositor::removeOutput);

    m_state = State::On;

    const auto windows = workspace()->windows();
    for (Window *window : windows) {
        window->setupCompositing();
    }

    // Sets also the 'effects' pointer.
    kwinApp()->createEffectsHandler(this, m_scene.get());

    Q_EMIT compositingToggled(true);

    if (m_releaseSelectionTimer.isActive()) {
        m_releaseSelectionTimer.stop();
    }
}

void WaylandCompositor::stop()
{
    if (m_state == State::Off || m_state == State::Stopping) {
        return;
    }
    m_state = State::Stopping;
    Q_EMIT aboutToToggleCompositing();

    m_releaseSelectionTimer.start();

    // Some effects might need access to effect windows when they are about to
    // be destroyed, for example to unreference deleted windows, so we have to
    // make sure that effect windows outlive effects.
    delete effects;
    effects = nullptr;

    if (Workspace::self()) {
        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            window->finishCompositing();
        }
        if (auto *con = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(con, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        }

        disconnect(workspace(), &Workspace::outputAdded, this, &WaylandCompositor::addOutput);
        disconnect(workspace(), &Workspace::outputRemoved, this, &WaylandCompositor::removeOutput);
    }

    if (m_backend->compositingType() == OpenGLCompositing) {
        // some layers need a context current for destruction
        static_cast<OpenGLBackend *>(m_backend.get())->makeCurrent();
    }

    const auto superlayers = m_superlayers;
    for (auto it = superlayers.begin(); it != superlayers.end(); ++it) {
        removeSuperLayer(*it);
    }

    m_scene.reset();
    m_cursorScene.reset();
    m_backend.reset();

    m_state = State::Off;
    Q_EMIT compositingToggled(false);
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

Output *WaylandCompositor::findOutput(RenderLoop *loop) const
{
    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        if (output->renderLoop() == loop) {
            return output;
        }
    }
    return nullptr;
}

void WaylandCompositor::addOutput(Output *output)
{
    Q_ASSERT(kwinApp()->operationMode() != Application::OperationModeX11);

    auto workspaceLayer = new RenderLayer(output->renderLoop());
    workspaceLayer->setDelegate(std::make_unique<SceneDelegate>(m_scene.get(), output));
    workspaceLayer->setGeometry(output->rect());
    connect(output, &Output::geometryChanged, workspaceLayer, [output, workspaceLayer]() {
        workspaceLayer->setGeometry(output->rect());
    });

    auto cursorLayer = new RenderLayer(output->renderLoop());
    cursorLayer->setVisible(false);
    if (m_backend->compositingType() == OpenGLCompositing) {
        cursorLayer->setDelegate(std::make_unique<CursorDelegateOpenGL>(output));
    } else {
        cursorLayer->setDelegate(std::make_unique<CursorDelegateQPainter>(output));
    }
    cursorLayer->setParent(workspaceLayer);
    cursorLayer->setSuperlayer(workspaceLayer);

    auto updateCursorLayer = [this, output, cursorLayer]() {
        static bool valid;
        static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR", &valid) == 1 && valid;

        const Cursor *cursor = Cursors::self()->currentCursor();
        const QRectF layerRect = output->mapFromGlobal(cursor->geometry());
        const auto outputLayer = m_backend->cursorLayer(output);
        if (!cursor->isOnOutput(output)) {
            if (outputLayer && outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer();
            }
            if (cursorLayer->isVisible()) {
                cursorLayer->setVisible(false);
                cursorLayer->addRepaintFull();
            }
            return;
        }
        const auto renderHardwareCursor = [&]() {
            if (!outputLayer || forceSoftwareCursor) {
                return false;
            }
            const QMatrix4x4 monitorMatrix = Output::logicalToNativeMatrix(output->rect(), output->scale(), output->transform());
            const QRectF nativeCursorRect = monitorMatrix.mapRect(layerRect);
            QSize bufferSize(std::ceil(nativeCursorRect.width()), std::ceil(nativeCursorRect.height()));
            if (const auto fixedSize = outputLayer->fixedSize()) {
                if (fixedSize->width() < bufferSize.width() || fixedSize->height() < bufferSize.height()) {
                    return false;
                }
                bufferSize = *fixedSize;
            }
            outputLayer->setPosition(nativeCursorRect.topLeft());
            outputLayer->setHotspot(monitorMatrix.map(cursor->hotspot()));
            outputLayer->setSize(bufferSize);
            if (auto beginInfo = outputLayer->beginFrame()) {
                const RenderTarget &renderTarget = beginInfo->renderTarget;

                RenderLayer renderLayer(output->renderLoop());
                renderLayer.setDelegate(std::make_unique<SceneDelegate>(m_cursorScene.get(), output));
                renderLayer.setOutputLayer(outputLayer);

                renderLayer.delegate()->prePaint();
                renderLayer.delegate()->paint(renderTarget, infiniteRegion());
                renderLayer.delegate()->postPaint();

                if (!outputLayer->endFrame(infiniteRegion(), infiniteRegion())) {
                    return false;
                }
            }
            outputLayer->setEnabled(true);
            return output->updateCursorLayer();
        };
        if (renderHardwareCursor()) {
            cursorLayer->setVisible(false);
        } else {
            if (outputLayer && outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer();
            }
            cursorLayer->setVisible(cursor->isOnOutput(output));
            cursorLayer->setGeometry(layerRect);
            cursorLayer->addRepaintFull();
        }
    };
    auto moveCursorLayer = [this, output, cursorLayer]() {
        const Cursor *cursor = Cursors::self()->currentCursor();
        const QRectF layerRect = output->mapFromGlobal(cursor->geometry());
        const QMatrix4x4 monitorMatrix = Output::logicalToNativeMatrix(output->rect(), output->scale(), output->transform());
        const QRectF nativeCursorRect = monitorMatrix.mapRect(layerRect);
        const auto outputLayer = m_backend->cursorLayer(output);
        bool hardwareCursor = false;
        if (outputLayer && outputLayer->isEnabled()) {
            outputLayer->setPosition(nativeCursorRect.topLeft());
            hardwareCursor = output->updateCursorLayer();
        }
        cursorLayer->setVisible(cursor->isOnOutput(output) && !hardwareCursor);
        cursorLayer->setGeometry(layerRect);
        cursorLayer->addRepaintFull();
    };
    updateCursorLayer();
    connect(output, &Output::geometryChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::currentCursorChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::hiddenChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::positionChanged, cursorLayer, moveCursorLayer);

    addSuperLayer(workspaceLayer);
}

void WaylandCompositor::removeOutput(Output *output)
{
    removeSuperLayer(m_superlayers[output->renderLoop()]);
}

} // namespace KWin

#include "moc_compositor_wayland.cpp"
