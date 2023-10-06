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
#include "libkwineffects/glplatform.h"
#include "main.h"
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
        if (GLPlatform::instance()->recommendedCompositor() < OpenGLCompositing) {
            qCDebug(KWIN_CORE) << "Driver does not recommend OpenGL compositing";
            return false;
        }
    }

    // We only support the OpenGL 2+ shader API, not GL_ARB_shader_objects
    if (!hasGLVersion(2, 0)) {
        qCDebug(KWIN_CORE) << "OpenGL 2.0 is not supported";
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneOpenGL>(backend.get());
    m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererOpenGL>());
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

        // Force qtquick to software rendering if kwin uses software rendering too.
        if (m_selectedCompositor == QPainterCompositing) {
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
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

void WaylandCompositor::addOutput(Output *output)
{
    if (output->isPlaceholder()) {
        return;
    }
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

    static bool valid;
    static const bool forceSoftwareCursor = qEnvironmentVariableIntValue("KWIN_FORCE_SW_CURSOR", &valid) == 1 && valid;

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
            const QMatrix4x4 monitorMatrix = Output::logicalToNativeMatrix(output->rect(), output->scale(), output->transform());
            QRectF nativeCursorRect = monitorMatrix.mapRect(outputLocalRect);
            QSize bufferSize(std::ceil(nativeCursorRect.width()), std::ceil(nativeCursorRect.height()));
            if (const auto fixedSize = outputLayer->fixedSize()) {
                if (fixedSize->width() < bufferSize.width() || fixedSize->height() < bufferSize.height()) {
                    return false;
                }
                bufferSize = *fixedSize;
                nativeCursorRect = monitorMatrix.mapRect(QRectF(outputLocalRect.topLeft(), QSizeF(bufferSize) / output->scale()));
            }
            outputLayer->setPosition(nativeCursorRect.topLeft());
            outputLayer->setHotspot(Output::logicalToNativeMatrix(QRectF(QPointF(), QSizeF(bufferSize) / output->scale()), output->scale(), output->transform()).map(cursor->hotspot()));
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
        if (outputLayer) {
            if (outputLayer->isEnabled()) {
                const QMatrix4x4 monitorMatrix = Output::logicalToNativeMatrix(output->rect(), output->scale(), output->transform());
                const QRectF nativeCursorRect = monitorMatrix.mapRect(QRectF(outputLocalRect.topLeft(), outputLayer->size() / output->scale()));
                outputLayer->setPosition(nativeCursorRect.topLeft());
                hardwareCursor = output->updateCursorLayer();
            } else if (!cursorLayer->isVisible() && !forceSoftwareCursor) {
                // this is for the case that the cursor wasn't visible because it was on a different output before
                hardwareCursor = updateCursorLayer();
            }
        }
        cursorLayer->setVisible(cursor->isOnOutput(output) && !hardwareCursor);
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
