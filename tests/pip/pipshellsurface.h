/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include "qwayland-xdg-shell.h"
#include "qwayland-xx-pip-v1.h"

class XdgWmBase : public QWaylandClientExtensionTemplate<XdgWmBase>, public QtWayland::xdg_wm_base
{
    Q_OBJECT

public:
    XdgWmBase();
};

class XXPipShell : public QWaylandClientExtensionTemplate<XXPipShell>, public QtWayland::xx_pip_shell_v1
{
    Q_OBJECT

public:
    XXPipShell();
};

class PipShellIntegration : public QtWaylandClient::QWaylandShellIntegration
{
public:
    PipShellIntegration();

    bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    QtWaylandClient::QWaylandShellSurface *createShellSurface(QtWaylandClient::QWaylandWindow *window) override;

    static void assignPipRole(QWindow *window);

private:
    std::unique_ptr<XdgWmBase> m_xdgWmBase;
    std::unique_ptr<XXPipShell> m_xxPipShell;
};

class PipShellSurface : public QtWaylandClient::QWaylandShellSurface, public QtWayland::xdg_surface, public QtWayland::xx_pip_v1
{
    Q_OBJECT

public:
    PipShellSurface(::xdg_surface *xdgSurface, ::xx_pip_v1 *xxPip, QtWaylandClient::QWaylandWindow *window);
    ~PipShellSurface() override;

    bool isExposed() const override;
    void applyConfigure() override;
    void setWindowGeometry(const QRect &rect) override;
    bool move(QtWaylandClient::QWaylandInputDevice *inputDevice) override;
    bool resize(QtWaylandClient::QWaylandInputDevice *inputDevice, Qt::Edges edges) override;

private:
    void xdg_surface_configure(uint32_t serial) override;
    void xx_pip_v1_closed() override;
    void xx_pip_v1_configure_bounds(int32_t width, int32_t height) override;
    void xx_pip_v1_configure_size(int32_t width, int32_t height) override;

    QSize m_pendingSize;
    bool m_configured = false;
};
