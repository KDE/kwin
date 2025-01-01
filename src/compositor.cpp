/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include "config-kwin.h"

#include "core/brightnessdevice.h"
#include "core/graphicsbufferview.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "cursorsource.h"
#include "dbusinterface.h"
#include "effect/effecthandler.h"
#include "ftrace.h"
#include "opengl/glplatform.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"
#include "scene/cursordelegate_opengl.h"
#include "scene/cursordelegate_qpainter.h"
#include "scene/cursorscene.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/itemrenderer_qpainter.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "scene/workspacescene.h"
#include "scene/workspacescene_opengl.h"
#include "scene/workspacescene_qpainter.h"
#include "utils/common.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KLocalizedString>
#include <QQuickWindow>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

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
#if KWIN_BUILD_X11
    // 2 sec which should be enough to restart the compositor.
    static const int compositorLostMessageDelay = 2000;
    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, &QTimer::timeout,
            this, &Compositor::deleteUnusedSupportProperties);
#endif

    // register DBus
    new CompositorDBusInterface(this);
    FTraceLogger::create();
}

Compositor::~Compositor()
{
#if KWIN_BUILD_X11
    deleteUnusedSupportProperties();
#endif
    s_compositor = nullptr;
    Q_EMIT aboutToDestroy();
    stop(); // this can't be called in the destructor of Compositor
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

void Compositor::addSuperLayer(RenderLayer *layer)
{
    m_superlayers.insert(layer->loop(), layer);
    connect(layer->loop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
}

void Compositor::removeSuperLayer(RenderLayer *layer)
{
    m_superlayers.remove(layer->loop());
    disconnect(layer->loop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
    delete layer;
}

#if KWIN_BUILD_X11
void Compositor::keepSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties.removeAll(atom);
}

void Compositor::removeSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties << atom;
    m_unusedSupportPropertyTimer.start();
}

void Compositor::deleteUnusedSupportProperties()
{
    if (m_state == State::Starting || m_state == State::Stopping) {
        // Currently still maybe restarting the compositor.
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (auto *con = kwinApp()->x11Connection()) {
        for (const xcb_atom_t &atom : std::as_const(m_unusedSupportProperties)) {
            // remove property from root window
            xcb_delete_property(con, kwinApp()->x11RootWindow(), atom);
        }
        m_unusedSupportProperties.clear();
    }
}
#endif

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

void Compositor::framePass(RenderLayer *layer, OutputFrame *frame)
{
    layer->delegate()->frame(frame);
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        framePass(sublayer, frame);
    }
}

void Compositor::prePaintPass(RenderLayer *layer, QRegion *damage)
{
    if (const QRegion repaints = layer->repaints(); !repaints.isEmpty()) {
        *damage += layer->mapToGlobal(repaints);
        layer->resetRepaints();
    }

    const QRegion repaints = layer->delegate()->prePaint();
    if (!repaints.isEmpty()) {
        *damage += layer->mapToGlobal(repaints);
    }

    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        if (sublayer->isVisible()) {
            prePaintPass(sublayer, damage);
        }
    }
}

void Compositor::postPaintPass(RenderLayer *layer)
{
    layer->delegate()->postPaint();
    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        if (sublayer->isVisible()) {
            postPaintPass(sublayer);
        }
    }
}

void Compositor::paintPass(RenderLayer *layer, const RenderTarget &renderTarget, const QRegion &region)
{
    layer->delegate()->paint(renderTarget, region);

    const auto sublayers = layer->sublayers();
    for (RenderLayer *sublayer : sublayers) {
        if (sublayer->isVisible()) {
            paintPass(sublayer, renderTarget, region);
        }
    }
}

bool Compositor::isActive()
{
    return m_state == State::On;
}

bool Compositor::compositingPossible() const
{
    return true;
}

QString Compositor::compositingNotPossibleReason() const
{
    return QString();
}

bool Compositor::openGLCompositingIsBroken() const
{
    return false;
}

void Compositor::inhibit(Window *window)
{
}

void Compositor::uninhibit(Window *window)
{
}

bool Compositor::attemptOpenGLCompositing()
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
    if (const auto openglBackend = qobject_cast<OpenGLBackend *>(m_backend.get())) {
        m_scene = std::make_unique<WorkspaceSceneOpenGL>(openglBackend);
        m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererOpenGL>(openglBackend->eglDisplayObject()));
    } else {
        const auto qpainterBackend = static_cast<QPainterBackend *>(m_backend.get());
        m_scene = std::make_unique<WorkspaceSceneQPainter>(qpainterBackend);
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
    kwinApp()->createEffectsHandler(this, m_scene.get());

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

static bool checkForBlackBackground(SurfaceItem *background)
{
    if (!background->pixmap()
        || !background->pixmap()->buffer()
        || !background->pixmap()->buffer()->shmAttributes()
        || background->pixmap()->buffer()->shmAttributes()->size != QSize(1, 1)) {
        return false;
    }
    const GraphicsBufferView view(background->pixmap()->buffer());
    if (!view.image()) {
        return false;
    }
    const QRgb rgb = view.image()->pixel(0, 0);
    const QVector3D encoded(qRed(rgb) / 255.0, qGreen(rgb) / 255.0, qBlue(rgb) / 255.0);
    const QVector3D nits = background->colorDescription().mapTo(encoded, ColorDescription(NamedColorimetry::BT709, TransferFunction(TransferFunction::linear), 100, 0, std::nullopt, std::nullopt), background->renderingIntent());
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
    fTraceDuration("Paint (", output->name(), ")");

    RenderLayer *superLayer = m_superlayers[renderLoop];
    superLayer->setOutputLayer(primaryLayer);

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

    if (primaryLayer->needsRepaint() || superLayer->needsRepaint()) {
        auto totalTimeQuery = std::make_unique<CpuRenderTimeQuery>();
        renderLoop->beginPaint();

        QRegion surfaceDamage = primaryLayer->repaints();
        primaryLayer->resetRepaints();
        prePaintPass(superLayer, &surfaceDamage);
        frame->setDamage(surfaceDamage);

        // slowly adjust the artificial HDR headroom for the next frame
        // note that this is only done for internal displays, because external displays usually apply slow animations to brightness changes
        if (!output->highDynamicRange() && output->brightnessDevice() && output->currentBrightness() && output->artificialHdrHeadroom() && output->isInternal() && output->colorProfileSource() != Output::ColorProfileSource::ICC) {
            const auto desiredHdrHeadroom = superLayer->delegate()->desiredHdrHeadroom();
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
        const bool tearing = (output->capabilities() & Output::Capability::Tearing) && options->allowTearing() && activeFullscreenItem && activeWindow->wantsTearing(activeFullscreenItem->presentationHint() == PresentationModeHint::Async);
        if (vrr) {
            frame->setPresentationMode(tearing ? PresentationMode::AdaptiveAsync : PresentationMode::AdaptiveSync);
        } else {
            frame->setPresentationMode(tearing ? PresentationMode::Async : PresentationMode::VSync);
        }

        const uint32_t planeCount = 1;
        if (const auto scanoutCandidates = superLayer->delegate()->scanoutCandidates(planeCount + 1); !scanoutCandidates.isEmpty()) {
            const auto sublayers = superLayer->sublayers();
            bool scanoutPossible = std::none_of(sublayers.begin(), sublayers.end(), [](RenderLayer *sublayer) {
                return sublayer->isVisible();
            });
            if (scanoutCandidates.size() > planeCount) {
                scanoutPossible &= checkForBlackBackground(scanoutCandidates.back());
            }
            if (scanoutPossible) {
                primaryLayer->setTargetRect(centerBuffer(output->transform().map(scanoutCandidates.front()->size()), output->modeSize()));
                directScanout = primaryLayer->importScanoutBuffer(scanoutCandidates.front(), frame);
                if (directScanout) {
                    // if present works, we don't want to touch the frame object again afterwards,
                    // so end the time query here instead of later
                    totalTimeQuery->end();
                    frame->addRenderTimeQuery(std::move(totalTimeQuery));
                    totalTimeQuery = std::make_unique<CpuRenderTimeQuery>();

                    directScanout &= m_backend->present(output, frame);
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
        if (!directScanout) {
            totalTimeQuery->end();
            frame->addRenderTimeQuery(std::move(totalTimeQuery));
        }
    }

    if (!directScanout) {
        if (!m_backend->present(output, frame)) {
            m_backend->repairPresentation(output);
        }
    }

    framePass(superLayer, frame.get());

    if ((frame->brightness() && std::abs(*frame->brightness() - output->brightnessSetting()) > 0.001)
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
    auto workspaceLayer = new RenderLayer(output->renderLoop());
    workspaceLayer->setDelegate(std::make_unique<SceneDelegate>(m_scene.get(), output));
    workspaceLayer->setGeometry(output->rectF());
    connect(output, &Output::geometryChanged, workspaceLayer, [output, workspaceLayer]() {
        workspaceLayer->setGeometry(output->rectF());
    });

    auto cursorLayer = new RenderLayer(output->renderLoop());
    cursorLayer->setVisible(false);
    if (m_backend->compositingType() == OpenGLCompositing) {
        cursorLayer->setDelegate(std::make_unique<CursorDelegateOpenGL>(m_cursorScene.get(), output));
    } else {
        cursorLayer->setDelegate(std::make_unique<CursorDelegateQPainter>(m_cursorScene.get(), output));
    }
    cursorLayer->setParent(workspaceLayer);
    cursorLayer->setSuperlayer(workspaceLayer);

    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR") == 1;

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
        const bool wasHardwareCursor = outputLayer && outputLayer->isEnabled();
        if (renderHardwareCursor()) {
            cursorLayer->setVisible(false);
            return true;
        } else {
            if (outputLayer) {
                outputLayer->setEnabled(false);
                if (wasHardwareCursor) {
                    output->updateCursorLayer();
                }
            }
            cursorLayer->setVisible(cursor->isOnOutput(output));
            cursorLayer->setGeometry(outputLocalRect);
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
    };
    updateCursorLayer();
    connect(output, &Output::geometryChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::currentCursorChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::hiddenChanged, cursorLayer, updateCursorLayer);
    connect(Cursors::self(), &Cursors::positionChanged, cursorLayer, moveCursorLayer);

    addSuperLayer(workspaceLayer);
}

void Compositor::removeOutput(Output *output)
{
    if (output->isPlaceholder()) {
        return;
    }
    removeSuperLayer(m_superlayers[output->renderLoop()]);
}

} // namespace KWin

#include "moc_compositor.cpp"
