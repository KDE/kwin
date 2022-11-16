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

namespace KWin
{
namespace Wayland
{
class WaylandBackend;

class WaylandOutput : public Output
{
    Q_OBJECT
public:
    WaylandOutput(const QString &name, WaylandBackend *backend);
    ~WaylandOutput() override;

    RenderLoop *renderLoop() const override;

    void init(const QSize &pixelSize, qreal scale);

    bool isReady() const;
    KWayland::Client::Surface *surface() const;

    void lockPointer(KWayland::Client::Pointer *pointer, bool lock);
    void resize(const QSize &pixelSize);
    void setDpmsMode(DpmsMode mode) override;
    void updateDpmsMode(DpmsMode dpmsMode);
    void updateEnabled(bool enabled);

Q_SIGNALS:
    void sizeChanged(const QSize &size);

private:
    void handleConfigure(const QSize &size, KWayland::Client::XdgShellSurface::States states, quint32 serial);
    void updateWindowTitle();

    std::unique_ptr<RenderLoop> m_renderLoop;
    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<KWayland::Client::XdgShellSurface> m_xdgShellSurface;
    std::unique_ptr<KWayland::Client::LockedPointer> m_pointerLock;
    std::unique_ptr<KWayland::Client::XdgDecoration> m_xdgDecoration;
    WaylandBackend *const m_backend;
    QTimer m_turnOffTimer;
    bool m_hasPointerLock = false;
    bool m_ready = false;
};

} // namespace Wayland
} // namespace KWin
