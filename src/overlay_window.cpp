/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "overlay_window.h"
#include "scene/surfaceitem_wayland.h"
#include "scene/windowitem.h"
#include "wayland/kde_overlay_shell_v1.h"
#include "window.h"

namespace KWin
{

WindowOverlay::WindowOverlay(OverlayShellSurfaceV1 *shell, Window *parent)
    : m_shell(shell)
    , m_parent(parent)
{
    // TODO if there's already an overlay for the parent, close the other one?
    connect(parent, &Window::closed, m_shell, &OverlayShellSurfaceV1::close);
    connect(shell, &OverlayShellSurfaceV1::destroyed, this, &WindowOverlay::destroy);
    connect(shell, &OverlayShellSurfaceV1::configureAcked, this, &WindowOverlay::ackConfigure);
    connect(shell->surface(), &SurfaceInterface::committed, this, &WindowOverlay::handleCommit);
    connect(parent, &Window::bufferGeometryChanged, this, &WindowOverlay::configure);
    configure();
}

WindowOverlay::~WindowOverlay()
{
}

void WindowOverlay::configure()
{
    const QSizeF newSize = m_parent->bufferGeometry().size();
    if (m_lastConfigureSize == newSize) {
        return;
    }
    m_serial++;
    m_shell->configure(m_serial, newSize);
}

void WindowOverlay::ackConfigure(uint32_t serial)
{
    m_lastAckedConfigure = serial;
    // TODO delay commits on the parent surface to sync with this, too?
}

void WindowOverlay::handleCommit()
{
    if (!m_shell->surface()->isMapped()) {
        m_item.reset();
        m_lastAckedConfigure.reset();
        return;
    }
    if (!m_lastAckedConfigure) {
        // TODO m_shell->protocolError();
        return;
    }
    if (!m_item) {
        m_item = std::make_unique<SurfaceItemWayland>(m_shell->surface(), m_parent->windowItem()->windowContainer());
        m_parent->windowItem()->setOverlay(m_item.get());
    }
}

void WindowOverlay::destroy()
{
    delete this;
}

}
#include "moc_overlay_window.cpp"
