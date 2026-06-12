/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overlayshellsurface.h"

#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>

OverlayShell::OverlayShell()
    : QWaylandClientExtensionTemplate<OverlayShell>(1)
{
    initialize();
    if (!isActive()) {
        qFatal("The kde overlay shell protocol is not supported by the compositor");
    }
}

OverlayShellIntegration::OverlayShellIntegration()
    : m_overlayShell(std::make_unique<OverlayShell>())
{
}

bool OverlayShellIntegration::initialize(QtWaylandClient::QWaylandDisplay *display)
{
    return m_overlayShell->isInitialized();
}

QtWaylandClient::QWaylandShellSurface *OverlayShellIntegration::createShellSurface(QtWaylandClient::QWaylandWindow *window)
{
    ::kde_overlay_surface_v1 *overlay = m_overlayShell->get_overlay_surface(window->wlSurface(), window->property(QStringLiteral("overlay parent")).toString());
    return new OverlayShellSurface(overlay, window);
}

void OverlayShellIntegration::assignOverlayRole(QWindow *window)
{
    window->create();

    auto waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    if (!waylandWindow) {
        return;
    }
    waylandWindow->setShellIntegration(this);
}

OverlayShellSurface::OverlayShellSurface(::kde_overlay_surface_v1 *overlay, QtWaylandClient::QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::kde_overlay_surface_v1(overlay)
{
}

OverlayShellSurface::~OverlayShellSurface()
{
    kde_overlay_surface_v1::destroy();
}

bool OverlayShellSurface::isExposed() const
{
    return m_configured;
}

void OverlayShellSurface::applyConfigure()
{
    QSize size = window()->windowContentGeometry().size();
    if (m_pendingSize.width() > 0) {
        size.setWidth(m_pendingSize.width());
    }
    if (m_pendingSize.height() > 0) {
        size.setHeight(m_pendingSize.height());
    }

    window()->resizeFromApplyConfigure(size);
}

void OverlayShellSurface::setWindowGeometry(const QRect &rect)
{
    // not supported
}

bool OverlayShellSurface::move(QtWaylandClient::QWaylandInputDevice *inputDevice)
{
    // moving doesn't make sense
    return false;
}

bool OverlayShellSurface::resize(QtWaylandClient::QWaylandInputDevice *inputDevice, Qt::Edges edges)
{
    // resizing isn't allowed
    return false;
}

void OverlayShellSurface::kde_overlay_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height)
{
    m_pendingSize = QSize(width, height);
    ack_configure(serial);

    if (!m_configured) {
        m_configured = true;
        applyConfigure();
    } else {
        window()->applyConfigureWhenPossible();
    }

    window()->updateExposure();
}

void OverlayShellSurface::kde_overlay_surface_v1_closed()
{
    QWindowSystemInterface::handleCloseEvent(window()->window());
}

#include "moc_overlayshellsurface.cpp"
