/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "compositor_wayland.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "effect/effecthandler.h"
#include "ftrace.h"
#include "main.h"
#include "opengl/glplatform.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"
#include "scene/cursordelegate_opengl.h"
#include "scene/cursordelegate_qpainter.h"
#include "scene/cursorscene.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/itemrenderer_qpainter.h"
#include "scene/workspacescene_opengl.h"
#include "scene/workspacescene_qpainter.h"
#include "window.h"
#include "workspace.h"

#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif
#include <KLocalizedString>

#include <QQuickWindow>

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
}

WaylandCompositor::~WaylandCompositor()
{
    Q_EMIT aboutToDestroy();
    stop(); // this can't be called in the destructor of Compositor
}

bool WaylandCompositor::attemptOpenGLCompositing()
{
    std::unique_ptr<OpenGLBackend> backend = kwinApp()->outputBackend()->createOpenGLBackend();
    if (!backend) {
        return false;
    }
    if (!backend->isFailed()) {
        backend->init();
    }
    if (backend->isFailed()) {
        return false;
    }

    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O2") == 0 || qstrcmp(forceEnv, "O2ES") == 0) {
            qCDebug(KWIN_CORE) << "OpenGL 2 compositing enforced by environment variable";
        } else {
            // OpenGL 2 disabled by environment variable
            return false;
        }
    } else {
        if (backend->openglContext()->glPlatform()->recommendedCompositor() < OpenGLCompositing) {
            qCDebug(KWIN_CORE) << "Driver does not recommend OpenGL compositing";
            return false;
        }
    }

    // We only support the OpenGL 2+ shader API, not GL_ARB_shader_objects
    if (!backend->openglContext()->hasVersion(Version(2, 0))) {
        qCDebug(KWIN_CORE) << "OpenGL 2.0 is not supported";
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneOpenGL>(backend.get());
    m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererOpenGL>(backend->eglDisplayObject()));
    m_backend = std::move(backend);

    qCDebug(KWIN_CORE) << "OpenGL compositing has been successfully initialized";
    return true;
}

bool WaylandCompositor::attemptQPainterCompositing()
{
    std::unique_ptr<QPainterBackend> backend(kwinApp()->outputBackend()->createQPainterBackend());
    if (!backend || backend->isFailed()) {
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneQPainter>(backend.get());
    m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererQPainter>());
    m_backend = std::move(backend);

    qCDebug(KWIN_CORE) << "QPainter compositing has been successfully initialized";
    return true;
}

void WaylandCompositor::start()
{
    if (kwinApp()->isTerminating()) {
        return;
    }
    if (m_state != State::Off) {
        return;
    }

    Q_EMIT aboutToToggleCompositing();
    m_state = State::Starting;

    // If compositing has been restarted, try to use the last used compositing type.
    const QList<CompositingType> availableCompositors = kwinApp()->outputBackend()->supportedCompositors();
    QList<CompositingType> candidateCompositors;

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
        case NoCompositing:
            qCDebug(KWIN_CORE) << "Starting without compositing...";
            stop = true;
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

        qCCritical(KWIN_CORE) << "The used windowing system requires compositing";
        qCCritical(KWIN_CORE) << "We are going to quit KWin now as it is broken";
        qApp->quit();
        return;
    }

    if (m_selectedCompositor == NoCompositing) {
        m_selectedCompositor = m_backend->compositingType();

        switch (m_selectedCompositor) {
        case NoCompositing:
            break;
        case OpenGLCompositing:
            QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
            break;
        case QPainterCompositing:
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
            break;
        }
    }

    Q_EMIT sceneCreated();

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
}

void WaylandCompositor::stop()
{
    if (m_state == State::Off || m_state == State::Stopping) {
        return;
    }
    m_state = State::Stopping;
    Q_EMIT aboutToToggleCompositing();

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

static QRect centerBuffer(const QSizeF &bufferSize, const QSize &modeSize)
{
    const double widthScale = bufferSize.width() / double(modeSize.width());
    const double heightScale = bufferSize.height() / double(modeSize.height());
    if (widthScale > heightScale) {
        const QSize size = (bufferSize / widthScale).toSize();
        const uint32_t yOffset = (modeSize.height() - size.height()) / 2;
        return QRect(QPoint(0, yOffset), size);
    } else {
        const QSize size = (bufferSize / heightScale).toSize();
        const uint32_t xOffset = (modeSize.width() - size.width()) / 2;
        return QRect(QPoint(xOffset, 0), size);
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
    auto frame = std::make_shared<OutputFrame>(renderLoop);

    bool directScanout = false;
    if (primaryLayer->needsRepaint() || superLayer->needsRepaint()) {
        renderLoop->beginPaint();

        QRegion surfaceDamage = primaryLayer->repaints();
        primaryLayer->resetRepaints();
        prePaintPass(superLayer, &surfaceDamage);
        frame->setDamage(surfaceDamage);

        Window *const activeWindow = workspace()->activeWindow();
        SurfaceItem *const activeFullscreenItem = activeWindow && activeWindow->isFullScreen() && activeWindow->isOnOutput(output) ? activeWindow->surfaceItem() : nullptr;
        frame->setContentType(activeWindow && activeFullscreenItem ? activeFullscreenItem->contentType() : ContentType::None);

        const bool wantsAdaptiveSync = activeWindow && activeWindow->isOnOutput(output) && activeWindow->wantsAdaptiveSync();
        const bool vrr = (output->capabilities() & Output::Capability::Vrr) && (output->vrrPolicy() == VrrPolicy::Always || (output->vrrPolicy() == VrrPolicy::Automatic && wantsAdaptiveSync));
        const bool tearing = (output->capabilities() & Output::Capability::Tearing) && options->allowTearing() && activeFullscreenItem && activeFullscreenItem->presentationHint() == PresentationModeHint::Async;
        if (vrr) {
            frame->setPresentationMode(tearing ? PresentationMode::AdaptiveAsync : PresentationMode::AdaptiveSync);
        } else {
            frame->setPresentationMode(tearing ? PresentationMode::Async : PresentationMode::VSync);
        }

        if (const auto scanoutCandidates = superLayer->delegate()->scanoutCandidates(1); !scanoutCandidates.isEmpty()) {
            const auto sublayers = superLayer->sublayers();
            const bool scanoutPossible = std::none_of(sublayers.begin(), sublayers.end(), [](RenderLayer *sublayer) {
                return sublayer->isVisible();
            });
            if (scanoutPossible) {
                primaryLayer->setTargetRect(centerBuffer(output->transform().map(scanoutCandidates[0]->size()), output->modeSize()));
                directScanout = primaryLayer->importScanoutBuffer(scanoutCandidates[0]);
                if (directScanout) {
                    directScanout &= m_backend->present(output, frame);
                    if (directScanout) {
                        primaryLayer->notifyScanoutSuccessful();
                    }
                }
            }
        } else {
            primaryLayer->notifyNoScanoutCandidate();
        }

        if (!directScanout) {
            primaryLayer->setTargetRect(QRect(QPoint(0, 0), output->modeSize()));
            if (auto beginInfo = primaryLayer->beginFrame()) {
                auto &[renderTarget, repaint] = beginInfo.value();

                const QRegion bufferDamage = surfaceDamage.united(repaint).intersected(superLayer->rect().toAlignedRect());

                paintPass(superLayer, renderTarget, bufferDamage);
                primaryLayer->endFrame(bufferDamage, surfaceDamage, frame.get());
            }
        }

        postPaintPass(superLayer);
    }

    if (!directScanout) {
        m_backend->present(output, frame);
    }

    framePass(superLayer, frame.get());

    // TODO: move this into the cursor layer
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(output->renderLoop()->lastPresentationTimestamp());
    if (!Cursors::self()->isCursorHidden()) {
        Cursor *cursor = Cursors::self()->currentCursor();
        if (cursor->geometry().intersects(output->geometry())) {
            cursor->markAsRendered(frameTime);
        }
    }
}

void WaylandCompositor::addOutput(Output *output)
{
    if (output->isPlaceholder()) {
        return;
    }
    auto workspaceLayer = new RenderLayer(output->renderLoop());
    workspaceLayer->setDelegate(std::make_unique<SceneDelegate>(m_scene.get(), output));
    workspaceLayer->setGeometry(output->rectF());
    connect(output, &Output::geometryChanged, workspaceLayer, [output, workspaceLayer]() {
        workspaceLayer->setGeometry(output->rectF());
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

    // Software cursor is forced for intel devices because there are screen stuttering issues with hardware cursor,
    // possibly a kernel driver bug. Remove the workaround when https://gitlab.freedesktop.org/drm/intel/-/issues/9571 is fixed.
    static bool forceSoftwareCursorIsSet;
    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR", &forceSoftwareCursorIsSet) == 1
        || (!forceSoftwareCursorIsSet && scene()->openglContext() && scene()->openglContext()->glPlatform()->isIntel());

    auto updateCursorLayer = [this, output, cursorLayer]() {
        const Cursor *cursor = Cursors::self()->currentCursor();
        const QRectF outputLocalRect = output->mapFromGlobal(cursor->geometry());
        const auto outputLayer = m_backend->cursorLayer(output);
        if (!cursor->isOnOutput(output)) {
            if (outputLayer && outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer();
            }
            cursorLayer->setVisible(false);
            return true;
        }
        const auto renderHardwareCursor = [&]() {
            if (!outputLayer || forceSoftwareCursor) {
                return false;
            }
            QRectF nativeCursorRect = output->transform().map(scaledRect(outputLocalRect, output->scale()), output->pixelSize());
            QSize bufferSize(std::ceil(nativeCursorRect.width()), std::ceil(nativeCursorRect.height()));
            if (const auto fixedSize = outputLayer->fixedSize()) {
                if (fixedSize->width() < bufferSize.width() || fixedSize->height() < bufferSize.height()) {
                    return false;
                }
                bufferSize = *fixedSize;
                nativeCursorRect = output->transform().map(QRectF(outputLocalRect.topLeft() * output->scale(), bufferSize), output->pixelSize());
            }
            outputLayer->setHotspot(output->transform().map(cursor->hotspot() * output->scale(), bufferSize));
            outputLayer->setTargetRect(QRect(nativeCursorRect.topLeft().toPoint(), bufferSize));
            if (auto beginInfo = outputLayer->beginFrame()) {
                const RenderTarget &renderTarget = beginInfo->renderTarget;

                RenderLayer renderLayer(output->renderLoop());
                renderLayer.setDelegate(std::make_unique<SceneDelegate>(m_cursorScene.get(), output));
                renderLayer.setOutputLayer(outputLayer);

                renderLayer.delegate()->prePaint();
                renderLayer.delegate()->paint(renderTarget, infiniteRegion());
                renderLayer.delegate()->postPaint();

                if (!outputLayer->endFrame(infiniteRegion(), infiniteRegion(), nullptr)) {
                    return false;
                }
            } else {
                return false;
            }
            outputLayer->setEnabled(true);
            return output->updateCursorLayer();
        };
        if (renderHardwareCursor()) {
            cursorLayer->setVisible(false);
            return true;
        } else {
            if (outputLayer && outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer();
            }
            cursorLayer->setVisible(cursor->isOnOutput(output));
            cursorLayer->setGeometry(outputLocalRect);
            cursorLayer->addRepaintFull();
            return false;
        }
    };
    auto moveCursorLayer = [this, output, cursorLayer, updateCursorLayer]() {
        const Cursor *cursor = Cursors::self()->currentCursor();
        const QRectF outputLocalRect = output->mapFromGlobal(cursor->geometry());
        const auto outputLayer = m_backend->cursorLayer(output);
        bool hardwareCursor = false;
        const bool shouldBeVisible = cursor->isOnOutput(output);
        if (outputLayer && !forceSoftwareCursor) {
            if (shouldBeVisible) {
                const bool enabledBefore = outputLayer->isEnabled();
                if (enabledBefore) {
                    // just move it
                    const QRectF nativeCursorRect = output->transform().map(QRectF(outputLocalRect.topLeft() * output->scale(), outputLayer->targetRect().size()), output->pixelSize());
                    outputLayer->setTargetRect(QRect(nativeCursorRect.topLeft().toPoint(), outputLayer->targetRect().size()));
                    outputLayer->setEnabled(true);
                    hardwareCursor = output->updateCursorLayer();
                    if (!hardwareCursor) {
                        outputLayer->setEnabled(false);
                        if (enabledBefore) {
                            output->updateCursorLayer();
                        }
                    }
                } else {
                    // do the full update
                    hardwareCursor = updateCursorLayer();
                }
            } else if (outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer();
            }
        }
        cursorLayer->setVisible(shouldBeVisible && !hardwareCursor);
        cursorLayer->setGeometry(outputLocalRect);
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
    if (output->isPlaceholder()) {
        return;
    }
    removeSuperLayer(m_superlayers[output->renderLoop()]);
}

} // namespace KWin

#include "moc_compositor_wayland.cpp"
