/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_output.h"
#include "color_manager.h"
#include "compositor.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/renderbackend.h"
#include "core/renderloop_p.h"
#include "wayland_backend.h"
#include "wayland_display.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgdecoration.h>

#include "wayland-fractional-scale-v1-client-protocol.h"
#include "wayland-presentation-time-client-protocol.h"
#include "wayland-tearing-control-v1-client-protocol.h"
#include "wayland-viewporter-client-protocol.h"
#include "workspace.h"

#include <KLocalizedString>

#include <QPainter>

#include <cmath>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

WaylandCursor::WaylandCursor(WaylandBackend *backend)
    : m_surface(backend->display()->compositor()->createSurface())
{
    if (auto viewporter = backend->display()->viewporter()) {
        m_viewport = wp_viewporter_get_viewport(viewporter, *m_surface);
    }
}

WaylandCursor::~WaylandCursor()
{
    if (m_viewport) {
        wp_viewport_destroy(m_viewport);
    }
}

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

void WaylandCursor::setEnabled(bool enable)
{
    if (m_enabled != enable) {
        m_enabled = enable;
        sync();
    }
}

void WaylandCursor::update(wl_buffer *buffer, const QSize &logicalSize, const QPoint &hotspot)
{
    if (m_buffer != buffer || m_size != logicalSize || m_hotspot != hotspot) {
        m_buffer = buffer;
        m_size = logicalSize;
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
        if (m_viewport) {
            wp_viewport_set_destination(m_viewport, m_size.width(), m_size.height());
        }
        m_surface->attachBuffer(m_buffer);
        m_surface->damageBuffer(QRect(0, 0, INT32_MAX, INT32_MAX));
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }

    if (m_pointer) {
        m_pointer->setCursor(m_surface.get(), m_hotspot);
    }
}

void WaylandOutput::handleFractionalScaleChanged(void *data, struct wp_fractional_scale_v1 *wp_fractional_scale_v1, uint32_t scale120)
{
    reinterpret_cast<WaylandOutput *>(data)->m_pendingScale = scale120 / 120.0;
}

const wp_fractional_scale_v1_listener WaylandOutput::s_fractionalScaleListener{
    .preferred_scale = &WaylandOutput::handleFractionalScaleChanged,
};

WaylandOutput::WaylandOutput(const QString &name, WaylandBackend *backend)
    : Output(backend)
    , m_renderLoop(std::make_unique<RenderLoop>(this))
    , m_surface(backend->display()->compositor()->createSurface())
    , m_xdgShellSurface(backend->display()->xdgShell()->createSurface(m_surface.get()))
    , m_backend(backend)
    , m_cursor(std::make_unique<WaylandCursor>(backend))
{
    if (KWayland::Client::XdgDecorationManager *manager = m_backend->display()->xdgDecorationManager()) {
        m_xdgDecoration.reset(manager->getToplevelDecoration(m_xdgShellSurface.get()));
        m_xdgDecoration->setMode(KWayland::Client::XdgDecoration::Mode::ServerSide);
    }

    Capabilities caps = Capability::Dpms;
    if (auto manager = backend->display()->tearingControl()) {
        caps |= Capability::Tearing;
        m_tearingControl = wp_tearing_control_manager_v1_get_tearing_control(manager, *m_surface);
    }
    if (auto manager = backend->display()->colorManager()) {
        const bool supportsMinFeatures = manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)
            && manager->supportsTransferFunction(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
        if (supportsMinFeatures) {
            m_colorSurface = wp_color_manager_v1_get_surface(manager->object(), *m_surface);
            m_colorSurfaceFeedback = std::make_unique<ColorSurfaceFeedback>(wp_color_manager_v1_get_surface_feedback(manager->object(), *m_surface));
            connect(m_colorSurfaceFeedback.get(), &ColorSurfaceFeedback::preferredColorChanged, this, &WaylandOutput::updateColor);
        }
    }
    if (auto manager = backend->display()->fractionalScale()) {
        m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale(manager, *m_surface);
        wp_fractional_scale_v1_add_listener(m_fractionalScale, &s_fractionalScaleListener, this);
    }
    if (auto viewporter = backend->display()->viewporter()) {
        m_viewport = wp_viewporter_get_viewport(viewporter, *m_surface);
    }
    setInformation(Information{
        .name = name,
        .model = name,
        .capabilities = caps,
    });

    m_turnOffTimer.setSingleShot(true);
    m_turnOffTimer.setInterval(dimAnimationTime());
    connect(&m_turnOffTimer, &QTimer::timeout, this, [this] {
        updateDpmsMode(DpmsMode::Off);
    });

    m_configureThrottleTimer.setSingleShot(true);
    connect(&m_configureThrottleTimer, &QTimer::timeout, this, [this]() {
        applyConfigure(m_pendingConfigureSize, m_pendingConfigureSerial);
    });

    connect(m_surface.get(), &KWayland::Client::Surface::frameRendered, this, [this]() {
        Q_ASSERT(m_frame);
        m_frame->presented(std::chrono::steady_clock::now().time_since_epoch(), PresentationMode::VSync);
        m_frame.reset();
    });

    updateWindowTitle();

    connect(m_xdgShellSurface.get(), &XdgShellSurface::configureRequested, this, &WaylandOutput::handleConfigure);
    connect(m_xdgShellSurface.get(), &XdgShellSurface::closeRequested, qApp, &QCoreApplication::quit);
    connect(this, &WaylandOutput::enabledChanged, this, &WaylandOutput::updateWindowTitle);
    connect(this, &WaylandOutput::dpmsModeChanged, this, &WaylandOutput::updateWindowTitle);
}

WaylandOutput::~WaylandOutput()
{
    if (m_presentationFeedback) {
        wp_presentation_feedback_destroy(m_presentationFeedback);
        m_presentationFeedback = nullptr;
    }
    if (m_tearingControl) {
        wp_tearing_control_v1_destroy(m_tearingControl);
        m_tearingControl = nullptr;
    }
    if (m_colorSurface) {
        wp_color_management_surface_v1_destroy(m_colorSurface);
        m_colorSurface = nullptr;
    }
    if (m_viewport) {
        wp_viewport_destroy(m_viewport);
        m_viewport = nullptr;
    }
    m_xdgDecoration.reset();
    m_xdgShellSurface.reset();
    m_surface.reset();
}

void WaylandOutput::updateColor()
{
    const auto &preferred = m_colorSurfaceFeedback->preferredColor();
    const auto tf = TransferFunction(TransferFunction::gamma22, preferred.transferFunction().minLuminance, preferred.transferFunction().maxLuminance);
    State next = m_state;
    next.colorDescription = ColorDescription{
        preferred.containerColorimetry(),
        tf,
        preferred.referenceLuminance(),
        preferred.minLuminance(),
        preferred.maxAverageLuminance(),
        preferred.maxHdrLuminance(),
    };
    next.originalColorDescription = next.colorDescription;
    next.blendingColor = next.colorDescription;
    // we don't actually know this, but we have to assume *something*
    next.layerBlendingColor = next.colorDescription;
    setState(next);
    if (m_colorSurface) {
        const auto imageDescription = m_backend->display()->colorManager()->createImageDescription(next.colorDescription);
        wp_color_management_surface_v1_set_image_description(m_colorSurface, imageDescription, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
        wp_image_description_v1_destroy(imageDescription);
    }
}

void WaylandOutput::setPrimaryBuffer(wl_buffer *buffer)
{
    m_presentationBuffer = buffer;
}

static void handleDiscarded(void *data,
                            struct wp_presentation_feedback *wp_presentation_feedback)
{
    reinterpret_cast<WaylandOutput *>(data)->frameDiscarded();
}

static void handlePresented(void *data,
                            struct wp_presentation_feedback *wp_presentation_feedback,
                            uint32_t tv_sec_hi,
                            uint32_t tv_sec_lo,
                            uint32_t tv_nsec,
                            uint32_t refresh,
                            uint32_t seq_hi,
                            uint32_t seq_lo,
                            uint32_t flags)
{
    const auto timestamp = std::chrono::seconds((uint64_t(tv_sec_hi) << 32) | tv_sec_lo) + std::chrono::nanoseconds(tv_nsec);
    uint32_t refreshRate = 60'000;
    if (refresh != 0) {
        refreshRate = 1'000'000'000'000 / refresh;
    }
    reinterpret_cast<WaylandOutput *>(data)->framePresented(timestamp, refreshRate);
}

static void handleSyncOutput(void *data, struct wp_presentation_feedback *, struct wl_output *)
{
    // intentionally ignored
}

static constexpr struct wp_presentation_feedback_listener s_presentationListener{
    .sync_output = handleSyncOutput,
    .presented = handlePresented,
    .discarded = handleDiscarded,
};

bool WaylandOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    auto cursorLayer = Compositor::self()->backend()->cursorLayer(this);
    if (layersToUpdate.contains(cursorLayer)) {
        if (m_hasPointerLock && cursorLayer->isEnabled()) {
            return false;
        }
        m_cursor->setEnabled(cursorLayer->isEnabled());
        // TODO also move the actual cursor image update here too...
    }
    if (!layersToUpdate.contains(Compositor::self()->backend()->primaryLayer(this))) {
        // we still need to commit the surface anyways, to get presentation feedback
        if (auto presentationTime = m_backend->display()->presentationTime()) {
            m_presentationFeedback = wp_presentation_feedback(presentationTime, *m_surface);
            wp_presentation_feedback_add_listener(m_presentationFeedback, &s_presentationListener, this);
            m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
        } else {
            m_surface->commit(KWayland::Client::Surface::CommitFlag::FrameCallback);
        }
        m_frame = frame;
        return true;
    }
    if (!m_presentationBuffer) {
        return false;
    }
    if (m_tearingControl) {
        if (frame->presentationMode() == PresentationMode::Async) {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);
        } else {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
        }
        m_renderLoop->setPresentationMode(frame->presentationMode());
    }
    if (m_viewport) {
        wp_viewport_set_destination(m_viewport, geometry().width(), geometry().height());
    }
    m_surface->attachBuffer(m_presentationBuffer);
    m_surface->damage(frame->damage());
    m_surface->setScale(1);
    m_presentationBuffer = nullptr;
    if (auto presentationTime = m_backend->display()->presentationTime()) {
        m_presentationFeedback = wp_presentation_feedback(presentationTime, *m_surface);
        wp_presentation_feedback_add_listener(m_presentationFeedback, &s_presentationListener, this);
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    } else {
        m_surface->commit(KWayland::Client::Surface::CommitFlag::FrameCallback);
    }
    m_frame = frame;
    Q_EMIT outputChange(frame->damage());
    return true;
}

void WaylandOutput::frameDiscarded()
{
    m_frame.reset();
    if (m_presentationFeedback) {
        wp_presentation_feedback_destroy(m_presentationFeedback);
        m_presentationFeedback = nullptr;
    }
}

void WaylandOutput::framePresented(std::chrono::nanoseconds timestamp, uint32_t refreshRate)
{
    if (refreshRate != this->refreshRate()) {
        m_refreshRate = refreshRate;
        const auto mode = std::make_shared<OutputMode>(pixelSize(), m_refreshRate);
        State next = m_state;
        next.modes = {mode};
        next.currentMode = mode;
        setState(next);
        m_renderLoop->setRefreshRate(m_refreshRate);
    }
    m_frame->presented(timestamp, PresentationMode::VSync);
    m_frame.reset();
    if (m_presentationFeedback) {
        wp_presentation_feedback_destroy(m_presentationFeedback);
        m_presentationFeedback = nullptr;
    }
}

void WaylandOutput::applyChanges(const OutputConfiguration &config)
{
    const auto props = config.constChangeSet(this);
    if (!props) {
        return;
    }
    State next = m_state;
    next.enabled = props->enabled.value_or(m_state.enabled);
    next.transform = props->transform.value_or(m_state.transform);
    next.position = props->pos.value_or(m_state.position);
    // intentionally ignored, as it would get overwritten
    // with the fractional scale protocol anyways
    // next.scale = props->scale.value_or(m_state.scale);
    next.desiredModeSize = props->desiredModeSize.value_or(m_state.desiredModeSize);
    next.desiredModeRefreshRate = props->desiredModeRefreshRate.value_or(m_state.desiredModeRefreshRate);
    next.uuid = props->uuid.value_or(m_state.uuid);
    next.replicationSource = props->replicationSource.value_or(m_state.replicationSource);
    setState(next);
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

bool WaylandOutput::updateCursorLayer(std::optional<std::chrono::nanoseconds> allowedVrrDelay)
{
    // the host compositor moves the cursor, there's nothing to do
    return true;
}

void WaylandOutput::init(const QSize &pixelSize, qreal scale, bool fullscreen)
{
    m_renderLoop->setRefreshRate(m_refreshRate);

    auto mode = std::make_shared<OutputMode>(pixelSize, m_refreshRate);

    State initialState;
    initialState.modes = {mode};
    initialState.currentMode = mode;
    initialState.scale = scale;
    setState(initialState);

    m_xdgShellSurface->setFullscreen(fullscreen);
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

void WaylandOutput::setDpmsMode(DpmsMode mode)
{
    if (mode == DpmsMode::Off) {
        if (!m_turnOffTimer.isActive()) {
            Q_EMIT aboutToTurnOff(std::chrono::milliseconds(m_turnOffTimer.interval()));
            m_turnOffTimer.start();
        }
    } else {
        m_turnOffTimer.stop();
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

void WaylandOutput::handleConfigure(const QSize &size, XdgShellSurface::States states, quint32 serial)
{
    if (!m_ready) {
        m_ready = true;

        applyConfigure(size, serial);
    } else {
        // Output resizing is a resource intensive task, so the configure events are throttled.
        m_pendingConfigureSerial = serial;
        m_pendingConfigureSize = size;

        if (!m_configureThrottleTimer.isActive()) {
            m_configureThrottleTimer.start(1000000 / m_state.currentMode->refreshRate());
        }
    }
}

void WaylandOutput::applyConfigure(const QSize &size, quint32 serial)
{
    m_xdgShellSurface->ackConfigure(serial);
    if (!size.isEmpty()) {
        auto mode = std::make_shared<OutputMode>(size * m_pendingScale, m_refreshRate);

        State next = m_state;
        next.modes = {mode};
        next.currentMode = mode;
        next.scale = m_pendingScale;
        setState(next);

        Q_EMIT m_backend->outputsQueried();
    }
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
                          "KDE Wayland Compositor %1", name());

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
        Q_EMIT m_backend->pointerLockChanged(true);
    });
    connect(m_pointerLock.get(), &LockedPointer::unlocked, this, [this]() {
        m_pointerLock.reset();
        m_hasPointerLock = false;
        updateWindowTitle();
        Q_EMIT m_backend->pointerLockChanged(false);
    });
}

}
}

#include "moc_wayland_output.cpp"
