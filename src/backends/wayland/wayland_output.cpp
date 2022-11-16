/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_output.h"
#include "core/renderloop_p.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_server.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgdecoration.h>

#include <KLocalizedString>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;
static const int s_refreshRate = 60000; // TODO: can we get refresh rate data from Wayland host?

WaylandOutput::WaylandOutput(const QString &name, WaylandBackend *backend)
    : Output(backend)
    , m_renderLoop(std::make_unique<RenderLoop>())
    , m_surface(backend->display()->compositor()->createSurface())
    , m_xdgShellSurface(backend->display()->xdgShell()->createSurface(m_surface.get()))
    , m_backend(backend)
{
    if (KWayland::Client::XdgDecorationManager *manager = m_backend->display()->xdgDecorationManager()) {
        m_xdgDecoration.reset(manager->getToplevelDecoration(m_xdgShellSurface.get()));
        m_xdgDecoration->setMode(KWayland::Client::XdgDecoration::Mode::ServerSide);
    }

    setInformation(Information{
        .name = name,
        .model = name,
        .capabilities = Capability::Dpms,
    });

    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        updateDpmsMode(DpmsMode::Off);
    });

    connect(m_surface.get(), &KWayland::Client::Surface::frameRendered, this, [this]() {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(renderLoop());
        renderLoopPrivate->notifyFrameCompleted(std::chrono::steady_clock::now().time_since_epoch());
    });

    updateWindowTitle();

    connect(m_xdgShellSurface.get(), &XdgShellSurface::configureRequested, this, &WaylandOutput::handleConfigure);
    connect(m_xdgShellSurface.get(), &XdgShellSurface::closeRequested, qApp, &QCoreApplication::quit);
    connect(this, &WaylandOutput::enabledChanged, this, &WaylandOutput::updateWindowTitle);
    connect(this, &WaylandOutput::dpmsModeChanged, this, &WaylandOutput::updateWindowTitle);

    connect(backend, &WaylandBackend::pointerLockSupportedChanged, this, &WaylandOutput::updateWindowTitle);
    connect(backend, &WaylandBackend::pointerLockChanged, this, [this](bool locked) {
        if (locked) {
            if (!m_hasPointerLock) {
                // some other output has locked the pointer
                // this surface can stop trying to lock the pointer
                lockPointer(nullptr, false);
                // set it true for the other surface
                m_hasPointerLock = true;
            }
        } else {
            // just try unlocking
            lockPointer(nullptr, false);
        }
        updateWindowTitle();
    });
}

WaylandOutput::~WaylandOutput()
{
    m_xdgDecoration.reset();
    m_xdgShellSurface.reset();
    m_surface.reset();
}

bool WaylandOutput::isReady() const
{
    return m_ready;
}

KWayland::Client::Surface *WaylandOutput::surface() const
{
    return m_surface.get();
}

RenderLoop *WaylandOutput::renderLoop() const
{
    return m_renderLoop.get();
}

void WaylandOutput::init(const QSize &pixelSize, qreal scale)
{
    m_renderLoop->setRefreshRate(s_refreshRate);

    auto mode = std::make_shared<OutputMode>(pixelSize, s_refreshRate);

    State initialState;
    initialState.modes = {mode};
    initialState.currentMode = mode;
    initialState.scale = scale;
    setState(initialState);

    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

void WaylandOutput::resize(const QSize &pixelSize)
{
    auto mode = std::make_shared<OutputMode>(pixelSize, s_refreshRate);

    State next = m_state;
    next.modes = {mode};
    next.currentMode = mode;
    setState(next);

    Q_EMIT m_backend->outputsQueried();
}

void WaylandOutput::setDpmsMode(DpmsMode mode)
{
    if (mode == DpmsMode::Off) {
        if (!m_turnOffTimer.isActive()) {
            Q_EMIT aboutToTurnOff(std::chrono::milliseconds(m_turnOffTimer.interval()));
            m_turnOffTimer.start();
        }
        m_backend->createDpmsFilter();
    } else {
        m_turnOffTimer.stop();
        m_backend->clearDpmsFilter();

        if (mode != dpmsMode()) {
            updateDpmsMode(mode);
            Q_EMIT wakeUp();
        }
    }
}

void WaylandOutput::updateDpmsMode(DpmsMode dpmsMode)
{
    State next = m_state;
    next.dpmsMode = dpmsMode;
    setState(next);
}

void WaylandOutput::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

void WaylandOutput::handleConfigure(const QSize &size, XdgShellSurface::States states, quint32 serial)
{
    m_xdgShellSurface->ackConfigure(serial);
    if (size.width() > 0 && size.height() > 0) {
        resize(size * scale());
        if (m_ready) {
            Q_EMIT sizeChanged(size);
        }
    }

    m_ready = true;
}

void WaylandOutput::updateWindowTitle()
{
    QString grab;
    if (m_hasPointerLock) {
        grab = i18n("Press right control to ungrab pointer");
    } else if (m_backend->display()->pointerConstraints()) {
        grab = i18n("Press right control key to grab pointer");
    }

    QString title = i18nc("Title of nested KWin Wayland with Wayland socket identifier as argument",
                          "KDE Wayland Compositor %1 (%2)", name(), waylandServer()->socketName());

    if (!isEnabled()) {
        title += i18n("- Output disabled");
    } else if (dpmsMode() != DpmsMode::On) {
        title += i18n("- Output dimmed");
    } else if (!grab.isEmpty()) {
        title += QStringLiteral(" — ") + grab;
    }
    m_xdgShellSurface->setTitle(title);
}

void WaylandOutput::lockPointer(Pointer *pointer, bool lock)
{
    if (!lock) {
        const bool surfaceWasLocked = m_pointerLock && m_hasPointerLock;
        m_pointerLock.reset();
        m_hasPointerLock = false;
        if (surfaceWasLocked) {
            Q_EMIT m_backend->pointerLockChanged(false);
        }
        return;
    }

    Q_ASSERT(!m_pointerLock);
    m_pointerLock.reset(m_backend->display()->pointerConstraints()->lockPointer(surface(), pointer, nullptr, PointerConstraints::LifeTime::OneShot));
    if (!m_pointerLock->isValid()) {
        m_pointerLock.reset();
        return;
    }
    connect(m_pointerLock.get(), &LockedPointer::locked, this, [this]() {
        m_hasPointerLock = true;
        Q_EMIT m_backend->pointerLockChanged(true);
    });
    connect(m_pointerLock.get(), &LockedPointer::unlocked, this, [this]() {
        m_pointerLock.reset();
        m_hasPointerLock = false;
        Q_EMIT m_backend->pointerLockChanged(false);
    });
}

}
}
