/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "plasmashell_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "display.h"
#include "surface_interface.h"

#include <QTimer>

#include <wayland-server.h>
#include <wayland-plasma-shell-server-protocol.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class PlasmaShellInterface::Private : public Global::Private
{
public:
    Private(PlasmaShellInterface *q, Display *d);

    QList<PlasmaShellSurfaceInterface*> surfaces;

private:
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource);

    PlasmaShellInterface *q;
    static const struct org_kde_plasma_shell_interface s_interface;
};

PlasmaShellInterface::Private::Private(PlasmaShellInterface *q, Display *d)
    : Global::Private(d, &org_kde_plasma_shell_interface, s_version)
    , q(q)
{
}

const struct org_kde_plasma_shell_interface PlasmaShellInterface::Private::s_interface = {
    createSurfaceCallback
};



class PlasmaShellSurfaceInterface::Private : public Resource::Private
{
public:
    Private(PlasmaShellSurfaceInterface *q, PlasmaShellInterface *shell, SurfaceInterface *surface, wl_resource *parentResource);

    SurfaceInterface *surface;
    QPoint m_globalPos;
    Role m_role;
    bool m_positionSet = false;
    PanelBehavior m_panelBehavior = PanelBehavior::AlwaysVisible;

private:
    // interface callbacks
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void setOutputCallback(wl_client *client, wl_resource *resource, wl_resource *output);
    static void setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y);
    static void setRoleCallback(wl_client *client, wl_resource *resource, uint32_t role);
    static void setPanelBehaviorCallback(wl_client *client, wl_resource *resource, uint32_t flag);

    void setPosition(const QPoint &globalPos);
    void setRole(uint32_t role);
    void setPanelBehavior(org_kde_plasma_surface_panel_behavior behavior);

    PlasmaShellSurfaceInterface *q_func() {
        return reinterpret_cast<PlasmaShellSurfaceInterface *>(q);
    }

    static const struct org_kde_plasma_surface_interface s_interface;
};

PlasmaShellInterface::PlasmaShellInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

PlasmaShellInterface::~PlasmaShellInterface() = default;

void PlasmaShellInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *shell = c->createResource(&org_kde_plasma_shell_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &s_interface, this, nullptr);
}

void PlasmaShellInterface::Private::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    auto s = reinterpret_cast<PlasmaShellInterface::Private*>(wl_resource_get_user_data(resource));
    s->createSurface(client, wl_resource_get_version(resource), id, SurfaceInterface::get(surface), resource);
}

void PlasmaShellInterface::Private::createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource)
{
    auto it = std::find_if(surfaces.constBegin(), surfaces.constEnd(),
        [surface](PlasmaShellSurfaceInterface *s) {
            return surface == s->surface();
        }
    );
    if (it != surfaces.constEnd()) {
        wl_resource_post_error(surface->resource(), WL_DISPLAY_ERROR_INVALID_OBJECT, "PlasmaShellSurface already created");
        return;
    }
    PlasmaShellSurfaceInterface *shellSurface = new PlasmaShellSurfaceInterface(q, surface, parentResource);
    surfaces << shellSurface;
    QObject::connect(shellSurface, &PlasmaShellSurfaceInterface::destroyed, q,
        [this, shellSurface] {
            surfaces.removeAll(shellSurface);
        }
    );
    shellSurface->d->create(display->getConnection(client), version, id);
    emit q->surfaceCreated(shellSurface);
}

/*********************************
 * ShellSurfaceInterface
 *********************************/
PlasmaShellSurfaceInterface::Private::Private(PlasmaShellSurfaceInterface *q, PlasmaShellInterface *shell, SurfaceInterface *surface, wl_resource *parentResource)
    : Resource::Private(q, shell, parentResource, &org_kde_plasma_surface_interface, &s_interface)
    , surface(surface)
{
}

const struct org_kde_plasma_surface_interface PlasmaShellSurfaceInterface::Private::s_interface = {
    destroyCallback,
    setOutputCallback,
    setPositionCallback,
    setRoleCallback,
    setPanelBehaviorCallback
};

PlasmaShellSurfaceInterface::PlasmaShellSurfaceInterface(PlasmaShellInterface *shell, SurfaceInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, shell, parent, parentResource), parent)
{
}

PlasmaShellSurfaceInterface::~PlasmaShellSurfaceInterface() = default;

SurfaceInterface *PlasmaShellSurfaceInterface::surface() const {
    Q_D();
    return d->surface;
}

PlasmaShellInterface *PlasmaShellSurfaceInterface::shell() const {
    Q_D();
    return reinterpret_cast<PlasmaShellInterface*>(d->global);
}

PlasmaShellSurfaceInterface::Private *PlasmaShellSurfaceInterface::d_func() const
{
    return reinterpret_cast<PlasmaShellSurfaceInterface::Private*>(d.data());
}

void PlasmaShellSurfaceInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void PlasmaShellSurfaceInterface::Private::setOutputCallback(wl_client *client, wl_resource *resource, wl_resource *output)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(output)
    // TODO: implement
}

void PlasmaShellSurfaceInterface::Private::setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->setPosition(QPoint(x, y));
}

void PlasmaShellSurfaceInterface::Private::setPosition(const QPoint &globalPos)
{
    if (m_globalPos == globalPos && m_positionSet) {
        return;
    }
    m_positionSet = true;
    m_globalPos = globalPos;
    Q_Q(PlasmaShellSurfaceInterface);
    emit q->positionChanged();
}

void PlasmaShellSurfaceInterface::Private::setRoleCallback(wl_client *client, wl_resource *resource, uint32_t role)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->setRole(role);
}

void PlasmaShellSurfaceInterface::Private::setRole(uint32_t role)
{
    Role r = Role::Normal;
    switch (role) {
    case ORG_KDE_PLASMA_SURFACE_ROLE_DESKTOP:
        r = Role::Desktop;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_PANEL:
        r = Role::Panel;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_ONSCREENDISPLAY:
        r = Role::OnScreenDisplay;
        break;
    case ORG_KDE_PLASMA_SURFACE_ROLE_NORMAL:
    default:
        r = Role::Normal;
        break;
    }
    if (r == m_role) {
        return;
    }
    m_role = r;
    Q_Q(PlasmaShellSurfaceInterface);
    emit q->roleChanged();
}

void PlasmaShellSurfaceInterface::Private::setPanelBehaviorCallback(wl_client *client, wl_resource *resource, uint32_t flag)
{
    auto s = cast<Private>(resource);
    Q_ASSERT(client == *s->client);
    s->setPanelBehavior(org_kde_plasma_surface_panel_behavior(flag));
}

void PlasmaShellSurfaceInterface::Private::setPanelBehavior(org_kde_plasma_surface_panel_behavior behavior)
{
    PanelBehavior newBehavior = PanelBehavior::AlwaysVisible;
    switch (behavior) {
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_AUTO_HIDE:
        newBehavior = PanelBehavior::AutoHide;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_WINDOWS_CAN_COVER:
        newBehavior = PanelBehavior::WindowsCanCover;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_WINDOWS_GO_BELOW:
        newBehavior = PanelBehavior::WindowsGoBelow;
        break;
    case ORG_KDE_PLASMA_SURFACE_PANEL_BEHAVIOR_ALWAYS_VISIBLE:
    default:
        break;
    }
    if (m_panelBehavior == newBehavior) {
        return;
    }
    m_panelBehavior = newBehavior;
    Q_Q(PlasmaShellSurfaceInterface);
    emit q->panelBehaviorChanged();
}

QPoint PlasmaShellSurfaceInterface::position() const
{
    Q_D();
    return d->m_globalPos;
}

PlasmaShellSurfaceInterface::Role PlasmaShellSurfaceInterface::role() const
{
    Q_D();
    return d->m_role;
}

bool PlasmaShellSurfaceInterface::isPositionSet() const
{
    Q_D();
    return d->m_positionSet;
}

PlasmaShellSurfaceInterface::PanelBehavior PlasmaShellSurfaceInterface::panelBehavior() const
{
    Q_D();
    return d->m_panelBehavior;
}

}
}
