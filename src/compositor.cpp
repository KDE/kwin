/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include "config-kwin.h"

#include "core/brightnessdevice.h"
#include "core/drmdevice.h"
#include "core/graphicsbufferview.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "cursorsource.h"
#include "dbusinterface.h"
#include "effect/effecthandler.h"
#include "ftrace.h"
#include "opengl/eglbackend.h"
#include "opengl/glplatform.h"
#include "qpainter/qpainterbackend.h"
#include "scene/cursorscene.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/itemrenderer_qpainter.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KCrash>
#if KWIN_BUILD_NOTIFICATIONS
#include <KLocalizedString>
#include <KNotification>
#endif

#include <QQuickWindow>

namespace KWin
{

Compositor *Compositor::create(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    auto *compositor = new Compositor(parent);
    s_compositor = compositor;
    return compositor;
}

Compositor *Compositor::s_compositor = nullptr;
Compositor *Compositor::self()
{
    return s_compositor;
}

Compositor::Compositor(QObject *workspace)
    : QObject(workspace)
{
    // register DBus
    new CompositorDBusInterface(this);
    FTraceLogger::create();
}

Compositor::~Compositor()
{
    Q_EMIT aboutToDestroy();
    stop(); // this can't be called in the destructor of Compositor
    s_compositor = nullptr;
}

Output *Compositor::findOutput(RenderLoop *loop) const
{
    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        if (output->renderLoop() == loop) {
            return output;
        }
    }
    return nullptr;
}

void Compositor::reinitialize()
{
    // Restart compositing
    stop();
    start();
}

void Compositor::handleFrameRequested(RenderLoop *renderLoop)
{
    composite(renderLoop);
}

bool Compositor::isActive()
{
    return m_state == State::On;
}

static QVariantHash collectCrashInformation(const EglBackend *backend)
{
    const GLPlatform *glPlatform = backend->openglContext()->glPlatform();

    QVariantHash gpuInformation;
    gpuInformation[QStringLiteral("api_type")] = QStringLiteral("OpenGL");
    gpuInformation[QStringLiteral("name")] = QString::fromUtf8(glPlatform->glRendererString());
    if (const auto pciInfo = backend->drmDevice()->pciDeviceInfo()) {
        gpuInformation[QStringLiteral("id")] = QString::number(pciInfo->device_id, 16);
        gpuInformation[QStringLiteral("vendor_id")] = QString::number(pciInfo->vendor_id, 16);
    }
    if (glPlatform->driverVersion().isValid()) {
        gpuInformation[QStringLiteral("version")] = glPlatform->driverVersion().toString();
    }

    return gpuInformation;
}

bool Compositor::attemptOpenGLCompositing()
{
    std::unique_ptr<EglBackend> backend = kwinApp()->outputBackend()->createOpenGLBackend();
    if (!backend) {
        return false;
    }
    if (!backend->isFailed()) {
        backend->init();
    }
    if (backend->isFailed()) {
        return false;
    }

    KCrash::setGPUData(collectCrashInformation(backend.get()));

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
    m_backend = std::move(backend);
    qCDebug(KWIN_CORE) << "OpenGL compositing has been successfully initialized";
    return true;
}

bool Compositor::attemptQPainterCompositing()
{
    std::unique_ptr<QPainterBackend> backend(kwinApp()->outputBackend()->createQPainterBackend());
    if (!backend || backend->isFailed()) {
        return false;
    }
    m_backend = std::move(backend);
    qCDebug(KWIN_CORE) << "QPainter compositing has been successfully initialized";
    return true;
}

void Compositor::createRenderer()
{
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
}

void Compositor::createScene()
{
    if (const auto eglBackend = qobject_cast<EglBackend *>(m_backend.get())) {
        m_scene = std::make_unique<WorkspaceScene>(std::make_unique<ItemRendererOpenGL>(eglBackend->eglDisplayObject()));
        m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererOpenGL>(eglBackend->eglDisplayObject()));
    } else {
        m_scene = std::make_unique<WorkspaceScene>(std::make_unique<ItemRendererQPainter>());
        m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererQPainter>());
    }
    Q_EMIT sceneCreated();
}

void Compositor::start()
{
    if (kwinApp()->isTerminating()) {
        return;
    }
    if (m_state != State::Off) {
        return;
    }

    Q_EMIT aboutToToggleCompositing();
    m_state = State::Starting;

    if (!m_backend) {
        createRenderer();
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

    createScene();

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }
    connect(workspace(), &Workspace::outputAdded, this, &Compositor::addOutput);
    connect(workspace(), &Workspace::outputRemoved, this, &Compositor::removeOutput);

    m_state = State::On;

    const auto windows = workspace()->windows();
    for (Window *window : windows) {
        window->setupCompositing();
    }

    // Sets also the 'effects' pointer.
    new EffectsHandler(this, m_scene.get());

    Q_EMIT compositingToggled(true);
}

void Compositor::stop()
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
        disconnect(workspace(), &Workspace::outputAdded, this, &Compositor::addOutput);
        disconnect(workspace(), &Workspace::outputRemoved, this, &Compositor::removeOutput);
    }

    if (m_backend->compositingType() == OpenGLCompositing) {
        // some layers need a context current for destruction
        static_cast<EglBackend *>(m_backend.get())->openglContext()->makeCurrent();
    }

    for (auto &[loop, layer] : m_primaryViews) {
        disconnect(loop, &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
    }

    m_primaryViews.clear();
    m_scene.reset();
    m_cursorScene.reset();
    m_backend.reset();

    m_state = State::Off;
    Q_EMIT compositingToggled(false);
}

static bool isTearingRequested(const Item *item)
{
    if (item->presentationHint() == PresentationModeHint::Async) {
        return true;
    }

    const auto childItems = item->childItems();
    return std::ranges::any_of(childItems, [](const Item *childItem) {
        return isTearingRequested(childItem);
    });
}

static bool checkForBlackBackground(SurfaceItem *background)
{
    if (!background->buffer()
        || (!background->buffer()->singlePixelAttributes() && !background->buffer()->shmAttributes())
        || background->buffer()->size() != QSize(1, 1)) {
        return false;
    }
    const GraphicsBufferView view(background->buffer());
    if (!view.image()) {
        return false;
    }
    const QRgb rgb = view.image()->pixel(0, 0);
    const QVector3D encoded(qRed(rgb) / 255.0, qGreen(rgb) / 255.0, qBlue(rgb) / 255.0);
    const QVector3D nits = background->colorDescription().mapTo(encoded, ColorDescription(Colorimetry::BT709, TransferFunction(TransferFunction::linear), 100, 0, std::nullopt, std::nullopt), background->renderingIntent());
    // below 0.1 nits, it shouldn't be noticeable that we replace it with black
    return nits.lengthSquared() <= (0.1 * 0.1);
}

void Compositor::composite(RenderLoop *renderLoop)
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
    const auto &primaryDelegate = m_primaryViews[renderLoop];
    fTraceDuration("Paint (", output->name(), ")");

    renderLoop->prepareNewFrame();
    auto frame = std::make_shared<OutputFrame>(renderLoop, std::chrono::nanoseconds(1'000'000'000'000 / output->refreshRate()));
    bool directScanout = false;
    std::optional<double> desiredArtificalHdrHeadroom;

    // brightness animations should be skipped when
    // - the output is new, and we didn't have the output configuration applied yet
    // - there's not enough steps to do a smooth animation
    // - the brightness device is external, most of them do an animation on their own
    if (!output->currentBrightness().has_value()
        || (!output->highDynamicRange() && output->brightnessDevice() && !output->isInternal())
        || (!output->highDynamicRange() && output->brightnessDevice() && output->brightnessDevice()->brightnessSteps() < 5)) {
        frame->setBrightness(output->brightnessSetting() * output->dimming());
    } else {
        constexpr double changePerSecond = 3;
        const double maxChangePerFrame = changePerSecond * 1'000.0 / renderLoop->refreshRate();
        // brightness perception is non-linear, gamma 2.2 encoding *roughly* represents that
        const double current = std::pow(*output->currentBrightness(), 1.0 / 2.2);
        frame->setBrightness(std::pow(std::clamp(std::pow(output->brightnessSetting() * output->dimming(), 1.0 / 2.2), current - maxChangePerFrame, current + maxChangePerFrame), 2.2));
    }

    if (primaryLayer->needsRepaint()) {
        auto totalTimeQuery = std::make_unique<CpuRenderTimeQuery>();

        const QRegion surfaceDamage = primaryLayer->repaints() | primaryDelegate->prePaint();
        primaryLayer->resetRepaints();
        frame->setDamage(surfaceDamage);

        // slowly adjust the artificial HDR headroom for the next frame
        // note that this is only done for internal displays, because external displays usually apply slow animations to brightness changes
        if (!output->highDynamicRange() && output->brightnessDevice() && output->currentBrightness() && output->isInternal()) {
            const auto desiredHdrHeadroom = output->edrPolicy() == Output::EdrPolicy::Always ? primaryDelegate->desiredHdrHeadroom() : 1.0;
            // just a rough estimate from the Framework 13 laptop. The less accurate this is, the more the screen will flicker during backlight changes
            constexpr double relativeLuminanceAtZeroBrightness = 0.04;
            // the higher this is, the more likely the user is to notice the change in backlight brightness
            // at the same time, if it's too low, it takes ages until the user sees the HDR effect
            constexpr double changePerSecond = 0.5;
            // to restrict HDR videos from using all the battery and burning your eyes
            // TODO make it a setting, and/or dependent on the power management state?
            constexpr double maxHdrHeadroom = 3.0;
            // = the headroom at 100% backlight
            const double maxPossibleHeadroom = (1 + relativeLuminanceAtZeroBrightness) / (relativeLuminanceAtZeroBrightness + *output->currentBrightness());
            desiredArtificalHdrHeadroom = std::clamp(desiredHdrHeadroom, 1.0, std::min(maxPossibleHeadroom, maxHdrHeadroom));
            const double changePerFrame = changePerSecond * double(frame->refreshDuration().count()) / 1'000'000'000;
            const double newHeadroom = std::clamp(*desiredArtificalHdrHeadroom, output->artificialHdrHeadroom() - changePerFrame, output->artificialHdrHeadroom() + changePerFrame);
            frame->setArtificialHdrHeadroom(newHeadroom);
        } else {
            frame->setArtificialHdrHeadroom(1);
        }

        Window *const activeWindow = workspace()->activeWindow();
        SurfaceItem *const activeFullscreenItem = activeWindow && activeWindow->isFullScreen() && activeWindow->isOnOutput(output) ? activeWindow->surfaceItem() : nullptr;
        frame->setContentType(activeWindow && activeFullscreenItem ? activeFullscreenItem->contentType() : ContentType::None);

        const bool wantsAdaptiveSync = activeWindow && activeWindow->isOnOutput(output) && activeWindow->wantsAdaptiveSync();
        const bool vrr = (output->capabilities() & Output::Capability::Vrr) && (output->vrrPolicy() == VrrPolicy::Always || (output->vrrPolicy() == VrrPolicy::Automatic && wantsAdaptiveSync));
        const bool tearing = (output->capabilities() & Output::Capability::Tearing) && options->allowTearing() && activeFullscreenItem && activeWindow->wantsTearing(isTearingRequested(activeFullscreenItem));
        if (vrr) {
            frame->setPresentationMode(tearing ? PresentationMode::AdaptiveAsync : PresentationMode::AdaptiveSync);
        } else {
            frame->setPresentationMode(tearing ? PresentationMode::Async : PresentationMode::VSync);
        }

        const uint32_t planeCount = 1;
        if (const auto scanoutCandidates = primaryDelegate->scanoutCandidates(planeCount + 1); !scanoutCandidates.isEmpty()) {
            bool scanoutPossible = scanoutCandidates.size() < planeCount || checkForBlackBackground(scanoutCandidates.back());
            if (scanoutPossible) {
                const auto geometry = scanoutCandidates.front()->mapToScene(QRectF(QPointF(0, 0), scanoutCandidates.front()->size())).translated(-output->geometryF().topLeft());
                primaryLayer->setTargetRect(output->transform().map(scaledRect(geometry, output->scale()), output->modeSize()).toRect());
                directScanout = primaryLayer->importScanoutBuffer(scanoutCandidates.front(), frame);
                if (directScanout) {
                    // if present works, we don't want to touch the frame object again afterwards,
                    // so end the time query here instead of later
                    totalTimeQuery->end();
                    frame->addRenderTimeQuery(std::move(totalTimeQuery));
                    totalTimeQuery = std::make_unique<CpuRenderTimeQuery>();

                    directScanout &= output->present(frame);
                }
            }
        } else {
            primaryLayer->notifyNoScanoutCandidate();
        }

        if (!directScanout) {
            primaryLayer->setTargetRect(QRect(QPoint(0, 0), output->modeSize()));
            if (auto beginInfo = primaryLayer->beginFrame()) {
                auto &[renderTarget, repaint] = beginInfo.value();

                const QRegion bufferDamage = surfaceDamage.united(repaint).intersected(output->rectF().toAlignedRect());

                primaryDelegate->paint(renderTarget, bufferDamage);
                primaryLayer->endFrame(bufferDamage, surfaceDamage, frame.get());
            }
        }

        primaryDelegate->postPaint();
        if (!directScanout) {
            totalTimeQuery->end();
            frame->addRenderTimeQuery(std::move(totalTimeQuery));
        }
    }

    if (!directScanout) {
        if (!output->present(frame)) {
            output->repairPresentation();
        }
    }

    primaryDelegate->frame(frame.get());

    if ((frame->brightness() && std::abs(*frame->brightness() - output->brightnessSetting() * output->dimming()) > 0.001)
        || (desiredArtificalHdrHeadroom && frame->artificialHdrHeadroom() && std::abs(*frame->artificialHdrHeadroom() - *desiredArtificalHdrHeadroom) > 0.001)) {
        // we're currently running an animation to change the brightness
        renderLoop->scheduleRepaint();
    }

    // TODO: move this into the cursor layer
    const auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(output->renderLoop()->lastPresentationTimestamp());
    if (!Cursors::self()->isCursorHidden()) {
        Cursor *cursor = Cursors::self()->currentCursor();
        if (cursor->geometry().intersects(output->geometry())) {
            if (CursorSource *source = cursor->source()) {
                source->frame(frameTime);
            }
        }
    }
}

void Compositor::addOutput(Output *output)
{
    if (output->isPlaceholder()) {
        return;
    }

    auto sceneView = std::make_unique<SceneView>(m_scene.get(), output, m_backend->primaryLayer(output));

    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR") == 1;

    auto updateCursorLayer = [this, output, sceneView = sceneView.get()]() {
        std::optional<std::chrono::nanoseconds> maxVrrCursorDelay;
        if (output->renderLoop()->activeWindowControlsVrrRefreshRate()) {
            const auto effectiveMinRate = output->minVrrRefreshRateHz().transform([](uint32_t value) {
                // this is intentionally using a tiny bit higher refresh rate than the minimum
                // so that slight differences in timing don't drop us below the minimum
                return value + 2;
            }).value_or(30);
            maxVrrCursorDelay = std::chrono::nanoseconds(1'000'000'000) / std::max(effectiveMinRate, 30u);
        }
        const Cursor *cursor = Cursors::self()->currentCursor();
        const QRectF outputLocalRect = output->mapFromGlobal(cursor->geometry());
        const auto outputLayer = m_backend->cursorLayer(output);
        if (!cursor->isOnOutput(output)) {
            if (outputLayer && outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer(maxVrrCursorDelay);
            }
            return true;
        }
        const auto renderHardwareCursor = [&]() {
            if (!outputLayer || forceSoftwareCursor) {
                return false;
            }
            QRectF nativeCursorRect = output->transform().map(scaledRect(outputLocalRect, output->scale()), output->pixelSize());
            QSize bufferSize(std::ceil(nativeCursorRect.width()), std::ceil(nativeCursorRect.height()));
            const auto recommendedSizes = outputLayer->recommendedSizes();
            if (!recommendedSizes.empty()) {
                auto bigEnough = recommendedSizes | std::views::filter([bufferSize](const auto &size) {
                    return size.width() >= bufferSize.width() && size.height() >= bufferSize.height();
                });
                const auto it = std::ranges::min_element(bigEnough, [](const auto &left, const auto &right) {
                    return left.width() * left.height() < right.width() * right.height();
                });
                if (it == bigEnough.end()) {
                    // no size found, this most likely won't work
                    return false;
                }
                bufferSize = *it;
                nativeCursorRect = output->transform().map(QRectF(outputLocalRect.topLeft() * output->scale(), bufferSize), output->pixelSize());
            }
            outputLayer->setHotspot(output->transform().map(cursor->hotspot() * output->scale(), bufferSize));
            outputLayer->setTargetRect(QRect(nativeCursorRect.topLeft().toPoint(), bufferSize));
            if (auto beginInfo = outputLayer->beginFrame()) {
                const RenderTarget &renderTarget = beginInfo->renderTarget;

                auto cursorView = std::make_unique<SceneView>(m_cursorScene.get(), output, outputLayer);

                cursorView->prePaint();
                cursorView->paint(renderTarget, infiniteRegion());
                cursorView->postPaint();

                if (!outputLayer->endFrame(infiniteRegion(), infiniteRegion(), nullptr)) {
                    return false;
                }
            } else {
                return false;
            }
            outputLayer->setEnabled(true);
            return output->updateCursorLayer(maxVrrCursorDelay);
        };
        const bool wasHardwareCursor = outputLayer && outputLayer->isEnabled();
        if (renderHardwareCursor()) {
            sceneView->hideItem(scene()->cursorItem());
            return true;
        } else {
            if (outputLayer) {
                outputLayer->setEnabled(false);
                if (wasHardwareCursor) {
                    output->updateCursorLayer(maxVrrCursorDelay);
                }
            }
            sceneView->showItem(scene()->cursorItem());
            return false;
        }
    };
    auto moveCursorLayer = [this, output, sceneView = sceneView.get(), updateCursorLayer]() {
        std::optional<std::chrono::nanoseconds> maxVrrCursorDelay;
        if (output->renderLoop()->activeWindowControlsVrrRefreshRate()) {
            // TODO use the output's minimum VRR range for this
            maxVrrCursorDelay = std::chrono::nanoseconds(1'000'000'000) / 30;
        }
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
                    hardwareCursor = output->updateCursorLayer(maxVrrCursorDelay);
                    if (!hardwareCursor) {
                        outputLayer->setEnabled(false);
                        if (enabledBefore) {
                            output->updateCursorLayer(maxVrrCursorDelay);
                        }
                    }
                } else {
                    // do the full update
                    hardwareCursor = updateCursorLayer();
                }
            } else if (outputLayer->isEnabled()) {
                outputLayer->setEnabled(false);
                output->updateCursorLayer(maxVrrCursorDelay);
            }
        }
        if (!shouldBeVisible) {
            return;
        }
        if (hardwareCursor) {
            sceneView->hideItem(scene()->cursorItem());
        } else {
            sceneView->showItem(scene()->cursorItem());
        }
    };
    updateCursorLayer();
    connect(output, &Output::geometryChanged, sceneView.get(), updateCursorLayer);
    connect(Cursors::self(), &Cursors::currentCursorChanged, sceneView.get(), updateCursorLayer);
    connect(Cursors::self(), &Cursors::hiddenChanged, sceneView.get(), updateCursorLayer);
    connect(Cursors::self(), &Cursors::positionChanged, sceneView.get(), moveCursorLayer);

    m_primaryViews[output->renderLoop()] = std::move(sceneView);
    connect(output->renderLoop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
}

void Compositor::removeOutput(Output *output)
{
    if (output->isPlaceholder()) {
        return;
    }
    disconnect(output->renderLoop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
    m_primaryViews.erase(output->renderLoop());
}

std::pair<std::shared_ptr<GLTexture>, ColorDescription> Compositor::textureForOutput(Output *output) const
{
    if (auto eglBackend = qobject_cast<EglBackend *>(m_backend.get())) {
        return eglBackend->textureForOutput(output);
    }

    return std::make_pair(nullptr, ColorDescription::sRGB);
}

} // namespace KWin

#include "moc_compositor.cpp"
