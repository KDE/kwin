/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "kde_overlay_shell_v1.h"
#include "display.h"
#include "surface_p.h"
#include "xdgforeign_v2.h"

namespace KWin
{

SurfaceRole OverlayShellManagerV1::s_role{QByteArrayLiteral("kde_overlay_surface_v1")};

OverlayShellManagerV1::OverlayShellManagerV1(Display *display, QObject *parent, XdgForeignV2Interface *foreign)
    : QObject(parent)
    , QtWaylandServer::kde_overlay_shell_manager_v1(*display, 1)
    , m_foreign(foreign)
{
}

void OverlayShellManagerV1::kde_overlay_shell_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OverlayShellManagerV1::kde_overlay_shell_manager_v1_get_overlay_surface(Resource *resource, uint32_t id, struct ::wl_resource *wlsurface, const QString &xdg_toplevel_handle)
{
    SurfaceInterface *surface = SurfaceInterface::get(wlsurface);
    if (const SurfaceRole *role = surface->role()) {
        if (role != &s_role) {
            wl_resource_post_error(resource->handle, error_role, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(&s_role);
    }
    SurfaceInterfacePrivate *priv = SurfaceInterfacePrivate::get(surface);
    if (priv->overlayShell) {
        wl_resource_post_error(wlsurface, error_role, "wl_surface already has an overlay shell surface");
        return;
    }
    auto shell = new OverlayShellSurfaceV1(resource->client(), id, resource->version(), surface);
    XdgExportedSurface *exported = m_foreign->exported(xdg_toplevel_handle);
    Q_EMIT overlayCreated(shell, exported ? exported->surface() : nullptr);
}

OverlayShellSurfaceV1::OverlayShellSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::kde_overlay_surface_v1(client, id, version)
    , m_surface(surface)
{
    SurfaceInterfacePrivate::get(surface)->overlayShell = this;
}

OverlayShellSurfaceV1::~OverlayShellSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->overlayShell = nullptr;
    }
}

SurfaceInterface *OverlayShellSurfaceV1::surface() const
{
    return m_surface;
}

void OverlayShellSurfaceV1::configure(uint32_t serial, const QSizeF &size)
{
    if (!m_surface) {
        return;
    }
    send_configure(serial,
                   std::round(size.width() * m_surface->compositorToClientScale()),
                   std::round(size.height() * m_surface->compositorToClientScale()));
}

void OverlayShellSurfaceV1::close()
{
    send_closed();
}

void OverlayShellSurfaceV1::kde_overlay_surface_v1_destroy_resource(Resource *resource)
{
    Q_EMIT destroyed();
    delete this;
}

void OverlayShellSurfaceV1::kde_overlay_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OverlayShellSurfaceV1::kde_overlay_surface_v1_ack_configure(Resource *resource, uint32_t serial)
{
    Q_EMIT configureAcked(serial);
}

}
#include "moc_kde_overlay_shell_v1.cpp"
