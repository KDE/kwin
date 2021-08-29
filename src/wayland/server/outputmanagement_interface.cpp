/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"

#include "outputmanagement_interface.h"
#include "outputconfiguration_interface.h"

#include "qwayland-server-outputmanagement.h"
#include <wayland-server.h>

#include <QHash>

namespace KWaylandServer
{
static const quint32 s_version = 4;

class OutputManagementInterfacePrivate : public QtWaylandServer::org_kde_kwin_outputmanagement
{
public:
    OutputManagementInterfacePrivate(OutputManagementInterface *_q, Display *display);

private:
    OutputManagementInterface *q;

protected:
    void org_kde_kwin_outputmanagement_create_configuration(Resource *resource, uint32_t id) override;
};

OutputManagementInterfacePrivate::OutputManagementInterfacePrivate(OutputManagementInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_outputmanagement(*display, s_version)
    , q(_q)
{
}

void OutputManagementInterfacePrivate::org_kde_kwin_outputmanagement_create_configuration(Resource *resource, uint32_t id)
{
    wl_resource *config_resource = wl_resource_create(resource->client(), &org_kde_kwin_outputconfiguration_interface, resource->version(), id);
    if (!config_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    new OutputConfigurationInterface(q, config_resource);
}

OutputManagementInterface::OutputManagementInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputManagementInterfacePrivate(this, display))
{
}

OutputManagementInterface::~OutputManagementInterface() = default;

}
