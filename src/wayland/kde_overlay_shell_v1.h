/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"
#include "surface.h"
#include "wayland/qwayland-server-kde-overlay-shell-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class OverlayShellSurfaceV1;
class XdgForeignV2Interface;

class KWIN_EXPORT OverlayShellManagerV1 : public QObject, private QtWaylandServer::kde_overlay_shell_manager_v1
{
    Q_OBJECT

public:
    explicit OverlayShellManagerV1(Display *display, QObject *parent, XdgForeignV2Interface *foreign);

Q_SIGNALS:
    void overlayCreated(OverlayShellSurfaceV1 *overlay, SurfaceInterface *parentSurface);

private:
    void kde_overlay_shell_manager_v1_destroy(Resource *resource) override;
    void kde_overlay_shell_manager_v1_get_overlay_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface, const QString &xdg_toplevel_handle) override;

    XdgForeignV2Interface *const m_foreign;
    static SurfaceRole s_role;
};

class OverlayShellSurfaceV1 : public QObject, private QtWaylandServer::kde_overlay_surface_v1
{
    Q_OBJECT

public:
    explicit OverlayShellSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~OverlayShellSurfaceV1() override;

    SurfaceInterface *surface() const;

    void configure(uint32_t serial, const QSizeF &size);
    void close();

Q_SIGNALS:
    void configureAcked(uint32_t serial);
    void destroyed();

private:
    void kde_overlay_surface_v1_destroy_resource(Resource *resource) override;
    void kde_overlay_surface_v1_destroy(Resource *resource) override;
    void kde_overlay_surface_v1_ack_configure(Resource *resource, uint32_t serial) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
