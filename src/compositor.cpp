/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include "config-kwin.h"

#include "core/backendoutput.h"
#include "core/brightnessdevice.h"
#include "core/drmdevice.h"
#include "core/graphicsbufferview.h"
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
#include "renderloopdrivenqanimationdriver.h"
#include "scene/cursoritem.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/itemrenderer_qpainter.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_wayland.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "utils/envvar.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include "utils/drm_format_helper.h"

#include <KCrash>
#if KWIN_BUILD_NOTIFICATIONS
#include <KLocalizedString>
#include <KNotification>
#endif

#include <QQuickWindow>
#include <optional>
#include <ranges>

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
    , m_allowOverlaysEnv(environmentVariableBoolValue("KWIN_USE_OVERLAYS"))
    , m_renderLoopDrivenAnimationDriver(new RenderLoopDrivenQAnimationDriver(this))
{
    // register DBus
    new CompositorDBusInterface(this);

    m_renderLoopDrivenAnimationDriver->install();
    connect(m_renderLoopDrivenAnimationDriver, &RenderLoopDrivenQAnimationDriver::started, this, [this]() {
        // foreach output, schedule repaint on render loop
        for (const auto &it : m_primaryViews) {
            RenderLoop *loop = it.first;
            loop->scheduleRepaint();
        }
    });

    FTraceLogger::create();
}

Compositor::~Compositor()
{
    Q_EMIT aboutToDestroy();
    stop(); // this can't be called in the destructor of Compositor
    s_compositor = nullptr;
}

BackendOutput *Compositor::findOutput(RenderLoop *loop) const
{
    const auto outputs = kwinApp()->outputBackend()->outputs();
    for (BackendOutput *output : outputs) {
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
    } else {
        m_scene = std::make_unique<WorkspaceScene>(std::make_unique<ItemRendererQPainter>());
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

    handleOutputsChanged();
    connect(workspace(), &Workspace::outputsChanged, this, &Compositor::handleOutputsChanged);
    connect(kwinApp()->outputBackend(), &OutputBackend::outputRemoved, this, &Compositor::removeOutput);

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
        disconnect(workspace(), &Workspace::outputsChanged, this, &Compositor::handleOutputsChanged);
        disconnect(kwinApp()->outputBackend(), &OutputBackend::outputRemoved, this, &Compositor::removeOutput);
    }

    if (m_backend->compositingType() == OpenGLCompositing) {
        // some layers need a context current for destruction
        static_cast<EglBackend *>(m_backend.get())->openglContext()->makeCurrent();
    }

    const auto loops = m_primaryViews | std::views::transform([](const auto &pair) {
        return pair.first;
    }) | std::ranges::to<QList>();
    for (RenderLoop *loop : loops) {
        removeOutput(findOutput(loop));
    }

    m_scene.reset();
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

static Rect mapGlobalLogicalToOutputDeviceCoordinates(const RectF &logicalGeometry, LogicalOutput *logicalOutput, BackendOutput *backendOutput)
{
    const Rect localDevice = logicalGeometry.scaled(backendOutput->scale()).rounded();
    const QPoint scaledOutputPos = (logicalOutput->geometryF().topLeft() * backendOutput->scale()).toPoint();
    return backendOutput->transform().map(localDevice.translated(backendOutput->deviceOffset() - scaledOutputPos), backendOutput->pixelSize());
}

static Rect mapItemToOutputDeviceCoordinates(Item *item, RenderView *view, LogicalOutput *logicalOutput, BackendOutput *backendOutput)
{
    const Rect scaledItemRect = item->mapToView(item->rect(), view).scaled(view->scale()).rounded();
    const QPoint scaledOutputPos = (logicalOutput->geometryF().topLeft() * view->scale()).toPoint();
    return backendOutput->transform().map(scaledItemRect.translated(backendOutput->deviceOffset() - scaledOutputPos), backendOutput->pixelSize());
}

static bool prepareDirectScanout(RenderView *view, LogicalOutput *logicalOutput, BackendOutput *backendOutput, const std::shared_ptr<OutputFrame> &frame)
{
    if (!view->isVisible()) {
        return false;
    }
    const auto layer = view->layer();
    const auto candidate = view->scanoutCandidate();
    if (!candidate) {
        layer->setScanoutCandidate(nullptr);
        return false;
    }
    SurfaceItemWayland *wayland = qobject_cast<SurfaceItemWayland *>(candidate);
    if (!wayland || !wayland->surface()) {
        return false;
    }
    const auto buffer = wayland->surface()->buffer();
    if (!buffer) {
        return false;
    }
    const auto attrs = buffer->dmabufAttributes();
    if (!attrs) {
        return false;
    }
    const bool tearing = frame->presentationMode() == PresentationMode::Async || frame->presentationMode() == PresentationMode::AdaptiveAsync;
    const auto formats = tearing ? layer->supportedAsyncDrmFormats() : layer->supportedDrmFormats();
    if (auto it = formats.find(attrs->format); it == formats.end() || !it->contains(attrs->modifier)) {
        layer->setScanoutCandidate(candidate);
        candidate->setScanoutHint(layer->scanoutDevice(), formats);
        return false;
    }
    layer->setTargetRect(mapItemToOutputDeviceCoordinates(candidate, view, logicalOutput, backendOutput));
    layer->setEnabled(true);
    layer->setSourceRect(candidate->bufferSourceBox());
    layer->setBufferTransform(candidate->bufferTransform());
    layer->setOffloadTransform(candidate->bufferTransform().combine(backendOutput->transform().inverted()));
    layer->setColor(candidate->colorDescription(), candidate->renderingIntent(), ColorPipeline::create(candidate->colorDescription(), backendOutput->layerBlendingColor(), candidate->renderingIntent()));
    const bool ret = layer->importScanoutBuffer(candidate->buffer(), frame);
    if (ret) {
        candidate->resetDamage();
        // ensure the pixmap is updated when direct scanout ends
        candidate->destroyTexture();
    }
    return ret;
}

static bool prepareRendering(RenderView *view, LogicalOutput *logicalOutput, BackendOutput *backendOutput, uint32_t requiredAlphaBits)
{
    if (!view->isVisible()) {
        return false;
    }
    auto nativeRect = mapGlobalLogicalToOutputDeviceCoordinates(view->viewport(), logicalOutput, backendOutput);
    // we need to render black bars for mirroring,
    // so add the relevant area to the source and target rect
    const QSize renderOffset = backendOutput->transform().map(QSize(view->renderOffset().x(), view->renderOffset().y()));
    nativeRect.adjust(-renderOffset.width(), -renderOffset.height(), renderOffset.width(), renderOffset.height());
    if ((nativeRect & Rect(QPoint(), backendOutput->modeSize())).isEmpty()) {
        return false;
    }

    const auto layer = view->layer();
    const double reference = backendOutput->colorDescription()->referenceLuminance();
    const double maxOutputLuminance = backendOutput->colorDescription()->maxHdrLuminance().value_or(reference);
    const double usedMaxLuminance = std::min(view->desiredHdrHeadroom() * reference, maxOutputLuminance);
    layer->setSourceRect(Rect(QPoint(0, 0), nativeRect.size()));
    layer->setTargetRect(nativeRect);
    layer->setHotspot(backendOutput->transform().map(view->hotspot() * view->scale(), nativeRect.size()));
    layer->setEnabled(true);
    layer->setOffloadTransform(OutputTransform::Normal);
    layer->setBufferTransform(backendOutput->transform());
    layer->setColor(backendOutput->layerBlendingColor()->withHdrMetadata(reference, usedMaxLuminance), RenderingIntent::AbsoluteColorimetricNoAdaptation, ColorPipeline{});
    layer->setRequiredAlphaBits(requiredAlphaBits);
    return layer->preparePresentationTest();
}

static bool renderLayer(RenderView *view, LogicalOutput *logicalOutput, BackendOutput *backendOutput, const std::shared_ptr<OutputFrame> &frame, const Region &surfaceDamage)
{
    auto beginInfo = view->layer()->beginFrame();
    if (!beginInfo) {
        return false;
    }
    auto &[renderTarget, repaint] = beginInfo.value();
    const Region bufferDamage = surfaceDamage.united(repaint).intersected(renderTarget.transformedRect());
    view->paint(renderTarget, view->renderOffset(), bufferDamage);
    return view->layer()->endFrame(bufferDamage, surfaceDamage, frame.get());
}

static OutputLayer *findLayer(std::span<OutputLayer *const> layers, OutputLayerType type)
{
    const auto it = std::ranges::find_if(layers, [type](OutputLayer *layer) {
        return layer->type() == type;
    });
    return it == layers.end() ? nullptr : *it;
}

/**
 * items and layers need to be sorted top to bottom
 */
static std::optional<std::unordered_map<OutputLayer *, Item *>> assignLayers(RenderView *sceneView, std::span<Item *const> items, std::span<OutputLayer *const> layers)
{
    if (layers.empty() || items.empty()) {
        return std::nullopt;
    }
    auto layerIt = layers.begin();
    int zpos = (*layerIt)->maxZpos();
    std::unordered_map<OutputLayer *, Item *> ret;
    auto itemIt = items.begin();
    for (; itemIt != items.end();) {
        Item *item = *itemIt;
        if (layerIt == layers.end()) {
            return std::nullopt;
        }
        OutputLayer *layer = *layerIt;
        const int nextZpos = std::min(zpos, layer->maxZpos());
        if (layer->minZpos() > nextZpos) {
            layerIt++;
            continue;
        }
        if (layer->type() == OutputLayerType::CursorOnly && qobject_cast<CursorItem *>(item) == nullptr) {
            layerIt++;
            continue;
        }
        const auto recommendedSizes = layer->recommendedSizes();
        if (!recommendedSizes.isEmpty()) {
            // it's likely that sizes other than the recommended ones won't work
            const bool compositingAllowed = qobject_cast<CursorItem *>(item) != nullptr;
            const RectF sceneRect = item->mapToView(compositingAllowed ?  item->boundingRect() : item->rect(), sceneView);
            const Rect deviceRect = sceneRect.translated(-sceneView->viewport().topLeft()).scaled(sceneView->scale()).rounded();
            const bool hasFittingSize = std::ranges::any_of(recommendedSizes, [compositingAllowed, deviceRect](const QSize &size) {
                if (compositingAllowed) {
                    return deviceRect.size().width() <= size.width()
                        && deviceRect.size().height() <= size.height();
                } else {
                    return deviceRect.size() == size;
                }
            });
            if (!hasFittingSize) {
                layerIt++;
                continue;
            }
        }
        layer->setZpos(nextZpos);
        ret[layer] = item;
        itemIt++;
        layerIt++;
        zpos = nextZpos - 1;
    }
    if (itemIt != items.end()) {
        // not all items were assigned, we need to composite
        return std::nullopt;
    }
    const bool primaryUsed = std::ranges::any_of(ret, [](const auto &pair) {
        return pair.first->type() == OutputLayerType::Primary;
    });
    if (primaryUsed) {
        return ret;
    }
    // the primary plane *must* be used, so swap a different layer out for it
    const auto primary = findLayer(layers, OutputLayerType::Primary);
    Q_ASSERT(primary);
    auto sortedLayers = ret | std::views::keys | std::ranges::to<QList>();
    std::ranges::sort(sortedLayers, [](OutputLayer *left, OutputLayer *right) {
        return left->zpos() < right->zpos();
    });
    for (ssize_t i = 0; i < sortedLayers.size(); i++) {
        // the primary layer has to fit in between the previous
        // and the next layer
        if (i > 0) {
            OutputLayer *previous = sortedLayers[i - 1];
            if (primary->maxZpos() < previous->zpos()) {
                continue;
            }
        }
        if (i < sortedLayers.size() - 1) {
            OutputLayer *next = sortedLayers[i + 1];
            if (primary->minZpos() > next->zpos()) {
                continue;
            }
        }
        primary->setZpos(std::clamp(sortedLayers[i]->zpos(), primary->minZpos(), primary->maxZpos()));
        Item *item = ret[sortedLayers[i]];
        ret.erase(sortedLayers[i]);
        ret[primary] = item;
        return ret;
    }
    return std::nullopt;
}

std::pair<QList<Compositor::LayerData>, bool> Compositor::setupLayers(SceneView *sceneView, LogicalOutput *logicalOutput,
                                                                      BackendOutput *backendOutput,
                                                                      const QList<OutputLayer *> &outputLayers,
                                                                      const std::unordered_map<OutputLayer *, Item *> &assignments,
                                                                      const std::shared_ptr<OutputFrame> &frame,
                                                                      SetupType type,
                                                                      std::unordered_set<OutputLayer *> &toUpdate)
{
    QList<OutputLayer *> unusedOutputLayers = outputLayers;
    QList<LayerData> layers;

    auto sortedLayers = assignments | std::views::keys | std::ranges::to<QList>();
    std::ranges::sort(sortedLayers, [](OutputLayer *left, OutputLayer *right) {
        return left->zpos() < right->zpos();
    });

    // every item below the scene needs to be treated as an underlay
    bool underlay = std::ranges::any_of(assignments | std::views::values, [this](Item *item) {
        return item == m_scene->containerItem();
    });
    for (auto it = sortedLayers.begin(); it != sortedLayers.end(); it++) {
        OutputLayer *layer = *it;
        Item *item = assignments.at(layer);
        const bool isCursor = qobject_cast<CursorItem *>(item) != nullptr;
        const bool isScene = item == m_scene->containerItem();
        if (isScene) {
            sceneView->setLayer(layer);
            underlay = false;
            layers.push_back(LayerData{
                .view = sceneView,
                .directScanout = false,
                .directScanoutOnly = false,
                .highPriority = true,
                .surfaceDamage = Region(),
                .requiredAlphaBits = 0,
            });
            if (it != sortedLayers.begin()) {
                // require more alpha bits for the scene if there is an underlay,
                // otherwise shadows from windows on top of the underlay will look terrible
                // TODO also make sure we still use more than 8 color bits when possible?
                layers.back().requiredAlphaBits = 8;
            }
            m_overlayViews[backendOutput->renderLoop()].erase(layer);
            unusedOutputLayers.removeOne(layer);
            continue;
        }
        auto &view = m_overlayViews[backendOutput->renderLoop()][layer];
        if (!view || view->item() != item) {
            if (isCursor) {
                // special handling for the cursor
                view = std::make_unique<ItemTreeView>(sceneView, item, logicalOutput, backendOutput, layer);
                connect(layer, &OutputLayer::repaintScheduled, view.get(), [logicalOutput, backendOutput, cursorView = view.get()]() {
                    // this just deals with moving the plane asynchronously, for improved latency.
                    // enabling, disabling and updating the cursor image still happen in composite()
                    const auto outputLayer = cursorView->layer();
                    if (!outputLayer->isEnabled()
                        || !outputLayer->deviceRepaints().isEmpty()
                        || !cursorView->isVisible()
                        || cursorView->needsRepaint()) {
                        // composite() handles this
                        return;
                    }
                    std::optional<std::chrono::nanoseconds> maxVrrCursorDelay;
                    if (backendOutput->renderLoop()->activeWindowControlsVrrRefreshRate()) {
                        const auto effectiveMinRate = backendOutput->minVrrRefreshRateHz().transform([](uint32_t value) {
                            // this is intentionally using a tiny bit higher refresh rate than the minimum
                            // so that slight differences in timing don't drop us below the minimum
                            return value + 2;
                        }).value_or(30);
                        maxVrrCursorDelay = std::chrono::nanoseconds(1'000'000'000) / std::max(effectiveMinRate, 30u);
                    }
                    outputLayer->setTargetRect(mapGlobalLogicalToOutputDeviceCoordinates(cursorView->viewport(), logicalOutput, backendOutput));
                    outputLayer->setEnabled(true);
                    if (backendOutput->presentAsync(outputLayer, maxVrrCursorDelay)) {
                        // prevent composite() from also pushing an update with the cursor layer
                        // to avoid adding cursor updates that are synchronized with primary layer updates
                        outputLayer->resetRepaints();
                    }
                });
            } else {
                view = std::make_unique<ItemView>(sceneView, item, logicalOutput, backendOutput, layer);
            }
        }
        layers.push_back(LayerData{
            .view = view.get(),
            .directScanout = !isCursor,
            .directScanoutOnly = !isCursor,
            .highPriority = isCursor,
            .surfaceDamage = Region(),
            .requiredAlphaBits = isCursor ? 8u : 0u,
        });
        view->setUnderlay(underlay);
        unusedOutputLayers.removeOne(layer);
    }

    // disable entirely unused output layers
    for (OutputLayer *layer : unusedOutputLayers) {
        m_overlayViews[backendOutput->renderLoop()].erase(layer);
        layer->setEnabled(false);
        // TODO only add the layer to `toUpdate` when necessary
        toUpdate.insert(layer);
    }

    // import buffers and prepare rendering
    for (auto &layer : layers) {
        if (type != SetupType::Fallback && prepareDirectScanout(layer.view, logicalOutput, backendOutput, frame)) {
            layer.directScanout = true;
        } else if (!layer.directScanoutOnly && prepareRendering(layer.view, logicalOutput, backendOutput, layer.requiredAlphaBits)) {
            layer.directScanout = false;
        } else {
            return std::make_pair(layers, false);
        }
    }
    return std::make_pair(layers, backendOutput->testPresentation(frame));
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

    BackendOutput *output = findOutput(renderLoop);
    LogicalOutput *logicalOutput = workspace()->findOutput(output);
    const auto primaryView = m_primaryViews[renderLoop].get();
    fTraceDuration("Paint (", output->name(), ")");

    // This must come first.
    renderLoop->prepareNewFrame();

    if (m_renderLoopDrivenAnimationDriver->isRunning()) {
        m_renderLoopDrivenAnimationDriver->advanceToNextFrame(renderLoop->nextPresentationTimestamp());
    }

    auto totalTimeQuery = std::make_unique<CpuRenderTimeQuery>();
    auto frame = std::make_shared<OutputFrame>(renderLoop, std::chrono::nanoseconds(1'000'000'000'000 / output->refreshRate()));
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
        // animate much slower for automatic brightness
        double changePerSecond = 3;
        if (output->lastBrightnessAdjustmentReason() == BackendOutput::BrightnessReason::AutomaticBrightness) {
            if (output->brightnessSetting() < output->currentBrightness()) {
                // brightness should be reduced slowly, or it'll be annoying
                changePerSecond = 0.1;
            } else {
                // but increased more quickly, so that you can still read your screen
                changePerSecond = 0.5;
            }
        }
        const double maxChangePerFrame = changePerSecond * 1'000.0 / renderLoop->refreshRate();
        // brightness perception is non-linear, gamma 2.2 encoding *roughly* represents that
        const double current = std::pow(*output->currentBrightness(), 1.0 / 2.2);
        frame->setBrightness(std::pow(std::clamp(std::pow(output->brightnessSetting() * output->dimming(), 1.0 / 2.2), current - maxChangePerFrame, current + maxChangePerFrame), 2.2));
    }

    Window *const activeWindow = workspace()->activeWindow();
    SurfaceItem *const activeFullscreenItem = activeWindow && activeWindow->isFullScreen() && activeWindow->frameGeometry().intersects(primaryView->viewport()) ? activeWindow->surfaceItem() : nullptr;
    frame->setContentType(activeWindow && activeFullscreenItem ? activeFullscreenItem->contentType() : ContentType::None);

    const bool wantsAdaptiveSync = activeWindow && activeWindow->frameGeometry().intersects(primaryView->viewport()) && activeWindow->wantsAdaptiveSync();
    const bool vrr = (output->capabilities() & BackendOutput::Capability::Vrr) && (output->vrrPolicy() == VrrPolicy::Always || (output->vrrPolicy() == VrrPolicy::Automatic && wantsAdaptiveSync));
    const bool tearing = (output->capabilities() & BackendOutput::Capability::Tearing) && options->allowTearing() && activeFullscreenItem && activeWindow->wantsTearing(isTearingRequested(activeFullscreenItem));
    if (vrr) {
        frame->setPresentationMode(tearing ? PresentationMode::AdaptiveAsync : PresentationMode::AdaptiveSync);
    } else {
        frame->setPresentationMode(tearing ? PresentationMode::Async : PresentationMode::VSync);
    }

    primaryView->prePaint();

    // slowly adjust the artificial HDR headroom for the next frame. Note that
    // - this has to happen (right) after prePaint, so that the scene's stacking order is valid
    // - this is only done for internal displays, because external displays usually apply slow animations to brightness changes
    if (!output->highDynamicRange() && output->brightnessDevice() && output->currentBrightness() && output->isInternal()) {
        const auto desiredHdrHeadroom = output->edrPolicy() == BackendOutput::EdrPolicy::Always ? primaryView->desiredHdrHeadroom() : 1.0;
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

    QList<OutputLayer *> unusedOutputLayers = m_backend->compatibleOutputLayers(output);

    const bool overlaysAllowed = m_allowOverlaysEnv.value_or(!output->overlayLayersLikelyBroken() && PROJECT_VERSION_PATCH >= 80);
    QList<OutputLayer *> allowedOutputLayers = unusedOutputLayers | std::views::filter([this, renderLoop, overlaysAllowed](OutputLayer *layer) {
        return (!m_brokenCursors.contains(renderLoop) || layer->type() != OutputLayerType::CursorOnly)
            && (overlaysAllowed || layer->type() != OutputLayerType::GenericLayer);
    }) | std::ranges::to<QList>();
    std::ranges::sort(allowedOutputLayers, [](OutputLayer *left, OutputLayer *right) {
        return left->maxZpos() > right->maxZpos();
    });
    const QList<Item *> layerCandidates = m_scene->layerCandidates(allowedOutputLayers.size());
    auto idealLayerAssignments = assignLayers(primaryView, layerCandidates, allowedOutputLayers);

    // fallback 1
    auto scenePlusCursor = layerCandidates | std::views::filter([](Item *item) {
        return qobject_cast<CursorItem *>(item) != nullptr;
    }) | std::ranges::to<QList>();
    scenePlusCursor.push_back(m_scene->containerItem());
    if (!idealLayerAssignments.has_value()) {
        idealLayerAssignments = assignLayers(primaryView, scenePlusCursor, allowedOutputLayers);
    }

    // fallback 2
    QList<Item *> sceneOnly{m_scene->containerItem()};
    if (!idealLayerAssignments.has_value()) {
        idealLayerAssignments = assignLayers(primaryView, sceneOnly, allowedOutputLayers);
        // this must always work
        Q_ASSERT(idealLayerAssignments.has_value());
    }

    std::unordered_set<OutputLayer *> toUpdate;
    auto [layers, result] = setupLayers(primaryView, logicalOutput, output, allowedOutputLayers, *idealLayerAssignments,
                                        frame, SetupType::Ideal, toUpdate);

    // test and downgrade the configuration until the test is successful
    if (!result) {
        // first, fall back to composited primary + hardware cursor, if that's not already done
        const bool fallback1 = layers.size() <= 2 && std::ranges::all_of(layers, [](const LayerData &layer) {
            return layer.highPriority;
        });
        if (!fallback1) {
            idealLayerAssignments = assignLayers(primaryView, scenePlusCursor, allowedOutputLayers);
            std::tie(layers, result) = setupLayers(primaryView, logicalOutput, output, allowedOutputLayers, *idealLayerAssignments,
                                                   frame, SetupType::Fallback, toUpdate);
        }

        // if this still doesn't work, fall back to primary only
        const bool fallback2 = layers.size() == 1 && layers.front().view == primaryView;
        if (!result && !fallback2) {
            idealLayerAssignments = assignLayers(primaryView, sceneOnly, allowedOutputLayers);
            std::tie(layers, result) = setupLayers(primaryView, logicalOutput, output, allowedOutputLayers, *idealLayerAssignments,
                                                   frame, SetupType::Fallback, toUpdate);
        }
    }

    const auto renderLayers = [&]() {
        for (auto &layer : layers) {
            if (!layer.view->layer()->needsRepaint()) {
                continue;
            }
            toUpdate.insert(layer.view->layer());
            layer.surfaceDamage |= layer.view->collectDamage();
            layer.surfaceDamage |= layer.view->layer()->deviceRepaints();
            layer.view->layer()->resetRepaints();
            if (layer.view->layer()->isEnabled() && !layer.directScanout) {
                result &= renderLayer(layer.view, logicalOutput, output, frame, layer.surfaceDamage);
                if (!result) {
                    qCWarning(KWIN_CORE, "Rendering a layer failed!");
                    break;
                }
            }
        }
    };

    // now actually render the layers that need rendering
    if (result) {
        // before rendering, enable and disable all the views that need it,
        // which may add repaints to other layers
        for (auto &layer : layers) {
            layer.view->setExclusive(layer.view->layer()->isEnabled());
        }

        // Note that effects may schedule repaints while rendering
        renderLoop->newFramePrepared();
        renderLayers();
    } else {
        renderLoop->newFramePrepared();
    }

    // NOTE that this does not count the time spent in BackendOutput::present,
    // but the drm backend, where that's necessary, tracks that time itself
    totalTimeQuery->end();
    frame->addRenderTimeQuery(std::move(totalTimeQuery));
    // TODO make Output::present take a set instead of a list
    if (result && !output->present(toUpdate | std::ranges::to<QList>(), frame)) {
        // legacy modesetting can't do (useful) presentation tests
        // and even with atomic modesetting, drivers are buggy and atomic tests
        // sometimes have false positives
        result = false;

        // same fallbacks as above:
        // first, fall back to composited primary + hardware cursor, if that's not already done
        const bool fallback1 = layers.size() <= 2 && std::ranges::all_of(layers, [](const LayerData &layer) {
            return layer.highPriority;
        });
        if (!fallback1) {
            idealLayerAssignments = assignLayers(primaryView, scenePlusCursor, allowedOutputLayers);
            std::tie(layers, result) = setupLayers(primaryView, logicalOutput, output, allowedOutputLayers, *idealLayerAssignments,
                                                   frame, SetupType::Fallback, toUpdate);
            if (result) {
                renderLayers();
                result = output->present(toUpdate | std::ranges::to<QList>(), frame);
            }
        }

        // if this still doesn't work, fall back to primary only
        const bool fallback2 = layers.size() == 1 && layers.front().view == primaryView;
        if (!result && !fallback2) {
            idealLayerAssignments = assignLayers(primaryView, sceneOnly, allowedOutputLayers);
            std::tie(layers, result) = setupLayers(primaryView, logicalOutput, output, allowedOutputLayers, *idealLayerAssignments,
                                                   frame, SetupType::Fallback, toUpdate);
            if (result) {
                renderLayers();
                result = output->present(toUpdate | std::ranges::to<QList>(), frame);
            }
            if (result && !fallback1) {
                // Disabling the cursor layer helped... so disable it permanently,
                // to prevent constantly attempting to render the hardware cursor again.
                // This should only ever happen with legacy modesetting, where
                // presentation can't be tested.
                // TODO move this to the drm backend?
                qCWarning(KWIN_CORE, "Disabling hardware cursor because of presentation failure");
                m_brokenCursors.insert(renderLoop);
            }
        }
    }

    m_scene->frame(primaryView, frame.get());
    primaryView->postPaint();

    // the layers have to stay valid until after postPaint, so this needs to happen after it
    if (!result) {
        qCWarning(KWIN_CORE, "Failed to find a working output layer configuration! Enabled layers:");
        for (const auto &layer : layers) {
            if (!layer.view->layer()->isEnabled()) {
                continue;
            }
            qCWarning(KWIN_CORE) << "src" << layer.view->layer()->sourceRect() << "-> dst" << layer.view->layer()->targetRect();
        }
        output->repairPresentation();
    }

    const bool forceRepaintForBrightness = (frame->brightness() && std::abs(*frame->brightness() - output->brightnessSetting() * output->dimming()) > 0.001)
        || (desiredArtificalHdrHeadroom && frame->artificialHdrHeadroom() && std::abs(*frame->artificialHdrHeadroom() - *desiredArtificalHdrHeadroom) > 0.001);

    const bool forceRepaintForOffscreenAnimations = m_renderLoopDrivenAnimationDriver->isRunning();

    if (forceRepaintForBrightness || forceRepaintForOffscreenAnimations) {
        // we're currently running an animation to change the brightness
        renderLoop->scheduleRepaint();
    }
}

void Compositor::handleOutputsChanged()
{
    for (auto &[loop, layer] : m_primaryViews) {
        disconnect(loop, &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
    }
    m_overlayViews.clear();
    m_primaryViews.clear();
    const auto outputs = kwinApp()->outputBackend()->outputs();
    for (BackendOutput *output : outputs | std::views::filter(&BackendOutput::isEnabled)) {
        addOutput(output);
    }
}

void Compositor::addOutput(BackendOutput *output)
{
    if (output->isPlaceholder()) {
        return;
    }
    assignOutputLayers(output);
    connect(output->renderLoop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
}

void Compositor::removeOutput(BackendOutput *output)
{
    if (output->isPlaceholder()) {
        return;
    }
    disconnect(output->renderLoop(), &RenderLoop::frameRequested, this, &Compositor::handleFrameRequested);
    m_overlayViews.erase(output->renderLoop());
    m_primaryViews.erase(output->renderLoop());
    m_brokenCursors.erase(output->renderLoop());
}

void Compositor::assignOutputLayers(BackendOutput *output)
{
    LogicalOutput *logical = workspace()->findOutput(output);
    Q_ASSERT(logical);
    const auto layers = m_backend->compatibleOutputLayers(output);
    const auto primaryLayer = findLayer(layers, OutputLayerType::Primary);
    Q_ASSERT(primaryLayer);
    auto &sceneView = m_primaryViews[output->renderLoop()];
    if (sceneView) {
        sceneView->setLayer(primaryLayer);
    } else {
        sceneView = std::make_unique<SceneView>(m_scene.get(), logical, output, primaryLayer);
        sceneView->setScale(output->scale());
        sceneView->setRenderOffset(output->deviceOffset());
        const auto updateViewport = [view = sceneView.get(), logical, output]() {
            // this matches how the renderer snaps elements to the pixel grid
            const Rect scaled = logical->geometryF().scaled(output->scale()).rounded();
            view->setViewport(scaled.scaled(1.0 / output->scale()));
        };
        updateViewport();
        connect(logical, &LogicalOutput::geometryChanged, sceneView.get(), updateViewport);
        connect(output, &BackendOutput::scaleChanged, sceneView.get(), [view = sceneView.get(), output]() {
            view->setScale(output->scale());
        });
        connect(output, &BackendOutput::deviceOffsetChanged, sceneView.get(), [view = sceneView.get(), output]() {
            view->setRenderOffset(output->deviceOffset());
        });
    }
    // will be re-assigned in the next composite() pass
    m_overlayViews.erase(output->renderLoop());
}

} // namespace KWin

#include "moc_compositor.cpp"
