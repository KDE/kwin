/********************************************************************
Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015 Sebastian Kügler <sebas@kde.org>

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
#include "outputmanagement_interface.h"
#include "outputconfiguration_interface.h"
#include "display.h"
#include "global_p.h"

#include <wayland-server.h>
#include "wayland-output-management-server-protocol.h"

#include <QHash>

namespace KWayland
{
namespace Server
{

class OutputManagementInterface::Private : public Global::Private
{
public:
    Private(OutputManagementInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }
    void createConfiguration(wl_client *client, wl_resource *resource, uint32_t id);

    static void createConfigurationCallback(wl_client *client, wl_resource *resource, uint32_t id);

    OutputManagementInterface *q;
    static const struct org_kde_kwin_outputmanagement_interface s_interface;
    static const quint32 s_version;

    QHash<wl_resource*, OutputConfigurationInterface*> configurationInterfaces;
};

const quint32 OutputManagementInterface::Private::s_version = 2;

const struct org_kde_kwin_outputmanagement_interface OutputManagementInterface::Private::s_interface = {
    createConfigurationCallback
};

OutputManagementInterface::OutputManagementInterface(Display *display, QObject *parent)
: Global(new Private(this, display), parent)
{
}

OutputManagementInterface::~OutputManagementInterface()
{
}

void OutputManagementInterface::Private::createConfigurationCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->createConfiguration(client, resource, id);
}

void OutputManagementInterface::Private::createConfiguration(wl_client* client, wl_resource* resource, uint32_t id)
{
    auto config = new OutputConfigurationInterface(q, resource);
    config->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!config->resource()) {
        wl_resource_post_no_memory(resource);
        delete config;
        return;
    }

    configurationInterfaces[resource] = config;
    connect(config, &QObject::destroyed, [this, resource] {
        configurationInterfaces.remove(resource);
    });
}

OutputManagementInterface::Private::Private(OutputManagementInterface *q, Display *d)
: Global::Private(d, &org_kde_kwin_outputmanagement_interface, s_version)
, q(q)
{
}

void OutputManagementInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_outputmanagement_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void OutputManagementInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

}
}
