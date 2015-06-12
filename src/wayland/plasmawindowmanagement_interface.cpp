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
#include "plasmawindowmanagement_interface.h"
#include "global_p.h"
#include "display.h"

#include <QVector>

#include <wayland-server.h>
#include <wayland-plasma-window-management-server-protocol.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class PlasmaWindowManagementInterface::Private : public Global::Private
{
public:
    Private(PlasmaWindowManagementInterface *q, Display *d);
    void sendShowingDesktopState();

    ShowingDesktopState state = ShowingDesktopState::Disabled;

private:
    static void unbind(wl_resource *resource);
    static void showDesktopCallback(wl_client *client, wl_resource *resource, uint32_t state);

    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendShowingDesktopState(wl_resource *r);

    PlasmaWindowManagementInterface *q;
    QVector<wl_resource*> resources;
    static const struct org_kde_plasma_window_management_interface s_interface;
};

PlasmaWindowManagementInterface::Private::Private(PlasmaWindowManagementInterface *q, Display *d)
    : Global::Private(d, &org_kde_plasma_window_management_interface, s_version)
    , q(q)
{
}

const struct org_kde_plasma_window_management_interface PlasmaWindowManagementInterface::Private::s_interface = {
    showDesktopCallback
};

void PlasmaWindowManagementInterface::Private::sendShowingDesktopState()
{
    for (wl_resource *r : resources) {
        sendShowingDesktopState(r);
    }
}

void PlasmaWindowManagementInterface::Private::sendShowingDesktopState(wl_resource *r)
{
    uint32_t s = 0;
    switch (state) {
    case ShowingDesktopState::Enabled:
        s = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED;
        break;
    case ShowingDesktopState::Disabled:
        s = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    org_kde_plasma_window_management_send_show_desktop_changed(r, s);
}

void PlasmaWindowManagementInterface::Private::showDesktopCallback(wl_client *client, wl_resource *resource, uint32_t state)
{
    Q_UNUSED(client)
    ShowingDesktopState s = ShowingDesktopState::Disabled;
    switch (state) {
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED:
        s = ShowingDesktopState::Enabled;
        break;
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED:
    default:
        s = ShowingDesktopState::Disabled;
        break;
    }
    emit reinterpret_cast<Private*>(wl_resource_get_user_data(resource))->q->requestChangeShowingDesktop(s);
}

PlasmaWindowManagementInterface::PlasmaWindowManagementInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

PlasmaWindowManagementInterface::~PlasmaWindowManagementInterface() = default;

void PlasmaWindowManagementInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *shell = c->createResource(&org_kde_plasma_window_management_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &s_interface, this, unbind);
    resources << shell;
}

void PlasmaWindowManagementInterface::Private::unbind(wl_resource *resource)
{
    auto wm = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    wm->resources.removeAll(resource);
}

void PlasmaWindowManagementInterface::setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState state)
{
    Q_D();
    if (d->state == state) {
        return;
    }
    d->state = state;
    d->sendShowingDesktopState();
}

PlasmaWindowManagementInterface::Private *PlasmaWindowManagementInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
