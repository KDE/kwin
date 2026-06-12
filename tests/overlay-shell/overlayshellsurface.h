/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtQmlIntegration>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include "qwayland-kde-overlay-shell-v1.h"

class OverlayShell : public QWaylandClientExtensionTemplate<OverlayShell>, public QtWayland::kde_overlay_shell_manager_v1
{
    Q_OBJECT

public:
    explicit OverlayShell();
};

class OverlayShellIntegration : public QObject, public QtWaylandClient::QWaylandShellIntegration
{
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT

public:
    explicit OverlayShellIntegration();

    bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    Q_INVOKABLE void assignOverlayRole(QWindow *window);

private:
    std::unique_ptr<OverlayShell> m_overlayShell;
};

class OverlayShellSurface : public QtWaylandClient::QWaylandShellSurface, public QtWayland::kde_overlay_surface_v1
{
    Q_OBJECT

public:
    explicit OverlayShellSurface(::kde_overlay_surface_v1 *overlay, QtWaylandClient::QWaylandWindow *window);
    ~OverlayShellSurface() override;

    bool isExposed() const override;
    void applyConfigure() override;
    void setWindowGeometry(const QRect &rect) override;
    bool move(QtWaylandClient::QWaylandInputDevice *inputDevice) override;
    bool resize(QtWaylandClient::QWaylandInputDevice *inputDevice, Qt::Edges edges) override;

private:
    void kde_overlay_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;
    void kde_overlay_surface_v1_closed() override;

    QSize m_pendingSize;
    bool m_configured = false;
};
