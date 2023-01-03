/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_output.h"
#include "composite.h"
#include "core/renderloop_p.h"
#include "cursorsource.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_egl_backend.h"
#include "wayland_qpainter_backend.h"
#include "wayland_server.h"

#include "kwingltexture.h"
#include "kwinglutils.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgdecoration.h>

#include "core/renderlayer.h"
#include "scene/cursorscene.h"

#include <KLocalizedString>

#include <QPainter>

#include <cmath>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;
static const int s_refreshRate = 60000; // TODO: can we get refresh rate data from Wayland host?

WaylandCursor::WaylandCursor(WaylandBackend *backend)
    : m_backend(backend)
    , m_surface(backend->display()->compositor()->createSurface())
{
}

WaylandCursor::~WaylandCursor() = default;

KWayland::Client::Pointer *WaylandCursor::pointer() const
{
    return m_pointer;
}

void WaylandCursor::setPointer(KWayland::Client::Pointer *pointer)
{
    if (m_pointer == pointer) {
        return;
    }
    m_pointer = pointer;
    if (m_pointer) {
        m_pointer->setCursor(m_surface.get(), m_hotspot);
    }
}

void WaylandCursor::enable()
{
    if (!m_enabled) {
        m_enabled = true;
        sync();
    }
}

void WaylandCursor::disable()
{
    if (m_enabled) {
        m_enabled = false;
        sync();
    }
}

void WaylandCursor::update(wl_buffer *buffer, qreal scale, const QPoint &hotspot)
{
    if (m_buffer != buffer || m_scale != scale || m_hotspot != hotspot) {
        m_buffer = buffer;
        m_scale = scale;
        m_hotspot = hotspot;

        sync();
    }
}

void WaylandCursor::sync()
{
    if (!m_enabled) {
        m_surface->attachBuffer(KWayland::Client::Buffer::Ptr());
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    } else {
        m_surface->attachBuffer(m_buffer);
        m_surface->setScale(std::ceil(m_scale));
        m_surface->damageBuffer(QRect(0, 0, INT32_MAX, INT32_MAX));
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    if (m_pointer) {
        m_pointer->setCursor(m_surface.get(), m_hotspot);
    }
}

WaylandOutput::WaylandOutput(const QString &name, WaylandBackend *backend)
    : Output(backend)
    , m_renderLoop(std::make_unique<RenderLoop>())
    , m_surface(backend->display()->compositor()->createSurface())
    , m_xdgShellSurface(backend->display()->xdgShell()->createSurface(m_surface.get()))
    , m_backend(backend)
    , m_cursor(std::make_unique<WaylandCursor>(backend))
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

WaylandCursor *WaylandOutput::cursor() const
{
    return m_cursor.get();
}

WaylandBackend *WaylandOutput::backend() const
{
    return m_backend;
}

RenderLoop *WaylandOutput::renderLoop() const
{
    return m_renderLoop.get();
}

bool WaylandOutput::setCursor(CursorSource *source)
{
    if (m_hasPointerLock) {
        return false;
    }

    if (WaylandEglBackend *backend = qobject_cast<WaylandEglBackend *>(Compositor::self()->backend())) {
        renderCursorOpengl(backend, source);
    } else if (WaylandQPainterBackend *backend = qobject_cast<WaylandQPainterBackend *>(Compositor::self()->backend())) {
        renderCursorQPainter(backend, source);
    }

    return true;
}

bool WaylandOutput::moveCursor(const QPoint &position)
{
    // The cursor position is controlled by the host compositor.
    return !m_hasPointerLock;
}

void WaylandOutput::renderCursorOpengl(WaylandEglBackend *backend, CursorSource *source)
{
    WaylandEglCursorLayer *cursorLayer = backend->cursorLayer(this);
    if (source) {
        cursorLayer->setSize(source->size());
        cursorLayer->setScale(scale());
        cursorLayer->setHotspot(source->hotspot());
    } else {
        cursorLayer->setSize(QSize());
        cursorLayer->setHotspot(QPoint());
    }

    std::optional<OutputLayerBeginFrameInfo> beginInfo = cursorLayer->beginFrame();
    if (!beginInfo) {
        return;
    }

    RenderTarget *renderTarget = &beginInfo->renderTarget;
    renderTarget->setDevicePixelRatio(scale());

    RenderLayer renderLayer(m_renderLoop.get());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));

    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(renderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    cursorLayer->endFrame(infiniteRegion(), infiniteRegion());
}

void WaylandOutput::renderCursorQPainter(WaylandQPainterBackend *backend, CursorSource *source)
{
    WaylandQPainterCursorLayer *cursorLayer = backend->cursorLayer(this);
    if (source) {
        cursorLayer->setSize(source->size());
        cursorLayer->setScale(scale());
        cursorLayer->setHotspot(source->hotspot());
    } else {
        cursorLayer->setSize(QSize());
        cursorLayer->setHotspot(QPoint());
    }

    std::optional<OutputLayerBeginFrameInfo> beginInfo = cursorLayer->beginFrame();
    if (!beginInfo) {
        return;
    }

    RenderTarget *renderTarget = &beginInfo->renderTarget;
    renderTarget->setDevicePixelRatio(scale());

    RenderLayer renderLayer(m_renderLoop.get());
    renderLayer.setDelegate(std::make_unique<SceneDelegate>(Compositor::self()->cursorScene()));

    renderLayer.delegate()->prePaint();
    renderLayer.delegate()->paint(renderTarget, infiniteRegion());
    renderLayer.delegate()->postPaint();

    cursorLayer->endFrame(infiniteRegion(), infiniteRegion());
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
            updateWindowTitle();
            m_cursor->enable();
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
        updateWindowTitle();
        m_cursor->disable();
        Q_EMIT m_backend->pointerLockChanged(true);
    });
    connect(m_pointerLock.get(), &LockedPointer::unlocked, this, [this]() {
        m_pointerLock.reset();
        m_hasPointerLock = false;
        updateWindowTitle();
        m_cursor->enable();
        Q_EMIT m_backend->pointerLockChanged(false);
    });
}

}
}
