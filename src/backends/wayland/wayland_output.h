/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"

#include <KWayland/Client/xdgshell.h>

#include <QObject>
#include <QTimer>

namespace KWayland
{
namespace Client
{
class Surface;
class Pointer;
class LockedPointer;
class XdgDecoration;
}
}

struct wl_buffer;
struct wp_presentation_feedback;

namespace KWin
{
class OutputFrame;

namespace Wayland
{
class WaylandBackend;

class WaylandCursor
{
public:
    explicit WaylandCursor(WaylandBackend *backend);
    ~WaylandCursor();

    KWayland::Client::Pointer *pointer() const;
    void setPointer(KWayland::Client::Pointer *pointer);

    void setEnabled(bool enable);
    void update(wl_buffer *buffer, qreal scale, const QPoint &hotspot);

private:
    void sync();

    KWayland::Client::Pointer *m_pointer = nullptr;
    std::unique_ptr<KWayland::Client::Surface> m_surface;
    wl_buffer *m_buffer = nullptr;
    QPoint m_hotspot;
    qreal m_scale = 1;
    bool m_enabled = true;
};

class WaylandOutput : public Output
{
    Q_OBJECT
public:
    WaylandOutput(const QString &name, WaylandBackend *backend);
    ~WaylandOutput() override;

    RenderLoop *renderLoop() const override;
    bool updateCursorLayer(std::optional<std::chrono::nanoseconds> allowedVrrDelay) override;

    void init(const QSize &pixelSize, qreal scale);

    bool isReady() const;
    KWayland::Client::Surface *surface() const;
    WaylandCursor *cursor() const;
    WaylandBackend *backend() const;

    void lockPointer(KWayland::Client::Pointer *pointer, bool lock);
    void resize(const QSize &pixelSize);
    void setDpmsMode(DpmsMode mode) override;
    void updateDpmsMode(DpmsMode dpmsMode);
    void updateEnabled(bool enabled);

    void present(const std::shared_ptr<OutputFrame> &frame);
    void setPrimaryBuffer(wl_buffer *buffer);

    void frameDiscarded();
    void framePresented(std::chrono::nanoseconds timestamp, uint32_t refreshRate);

private:
    void handleConfigure(const QSize &size, KWayland::Client::XdgShellSurface::States states, quint32 serial);
    void updateWindowTitle();
    void applyConfigure(const QSize &size, quint32 serial);

    std::unique_ptr<RenderLoop> m_renderLoop;
    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<KWayland::Client::XdgShellSurface> m_xdgShellSurface;
    std::unique_ptr<KWayland::Client::LockedPointer> m_pointerLock;
    std::unique_ptr<KWayland::Client::XdgDecoration> m_xdgDecoration;
    WaylandBackend *const m_backend;
    std::unique_ptr<WaylandCursor> m_cursor;
    QTimer m_turnOffTimer;
    bool m_hasPointerLock = false;
    bool m_ready = false;
    std::shared_ptr<OutputFrame> m_frame;
    wl_buffer *m_presentationBuffer = nullptr;
    quint32 m_pendingConfigureSerial = 0;
    QSize m_pendingConfigureSize;
    QTimer m_configureThrottleTimer;
    wp_presentation_feedback *m_presentationFeedback = nullptr;
    uint32_t m_refreshRate = 60'000;
};

} // namespace Wayland
} // namespace KWin
