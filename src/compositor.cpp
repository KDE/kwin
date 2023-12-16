/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "dbusinterface.h"
#include "ftrace.h"
#include "scene/cursorscene.h"
#include "scene/surfaceitem.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

namespace KWin
{

Compositor *Compositor::s_compositor = nullptr;
Compositor *Compositor::self()
{
    return s_compositor;
}

Compositor::Compositor(QObject *workspace)
    : QObject(workspace)
{
    // 2 sec which should be enough to restart the compositor.
    static const int compositorLostMessageDelay = 2000;

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, &QTimer::timeout,
            this, &Compositor::deleteUnusedSupportProperties);

    // Delay the call to start by one event cycle.
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    QTimer::singleShot(0, this, &Compositor::start);

    // register DBus
    new CompositorDBusInterface(this);
    FTraceLogger::create();
}

Compositor::~Compositor()
{
    deleteUnusedSupportProperties();
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

uint Compositor::outputFormat(Output *output)
{
    OutputLayer *primaryLayer = m_backend->primaryLayer(output);
    return primaryLayer->format();
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
    auto frame = std::make_shared<OutputFrame>(renderLoop);

    if (superLayer->needsRepaint()) {
        renderLoop->beginPaint();

        QRegion surfaceDamage = primaryLayer->repaints();
        primaryLayer->resetRepaints();
        prePaintPass(superLayer, &surfaceDamage);

        SurfaceItem *scanoutCandidate = superLayer->delegate()->scanoutCandidate();
        renderLoop->setFullscreenSurface(scanoutCandidate);
        frame->setContentType(scanoutCandidate ? scanoutCandidate->contentType() : ContentType::None);

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
            if (auto beginInfo = primaryLayer->beginFrame()) {
                auto &[renderTarget, repaint] = beginInfo.value();

                const QRegion bufferDamage = surfaceDamage.united(repaint).intersected(superLayer->rect().toAlignedRect());

                paintPass(superLayer, renderTarget, bufferDamage);
                primaryLayer->endFrame(bufferDamage, surfaceDamage);
            }
        }

        postPaintPass(superLayer);
    }

    m_backend->present(output, frame);

    framePass(superLayer, frame.get());

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

} // namespace KWin

#include "moc_compositor.cpp"
