/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include "config-kwin.h"

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
#include "scene/surfaceitem_wayland.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
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

} // namespace KWin

#include "moc_compositor.cpp"
