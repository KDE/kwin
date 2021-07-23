/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "outputmanagement_v2_interface.h"
#include "outputconfiguration_v2_interface.h"

#include <wayland-server.h>
#include "qwayland-server-kde-output-management-v2.h"

#include <QHash>

namespace KWaylandServer
{

static const quint32 s_version = 1;

class OutputManagementV2InterfacePrivate : public QtWaylandServer::kde_output_management_v2
{
public:
    OutputManagementV2InterfacePrivate(OutputManagementV2Interface *_q, Display *display);

private:
    OutputManagementV2Interface *q;

protected:
    void kde_output_management_v2_create_configuration(Resource *resource, uint32_t id) override;
};

OutputManagementV2InterfacePrivate::OutputManagementV2InterfacePrivate(OutputManagementV2Interface *_q, Display *display)
    : QtWaylandServer::kde_output_management_v2(*display, s_version)
    , q(_q)
{
}

void OutputManagementV2InterfacePrivate::kde_output_management_v2_create_configuration(Resource *resource, uint32_t id)
{
    wl_resource *config_resource = wl_resource_create(resource->client(), &kde_output_configuration_v2_interface, resource->version(), id);
    if (!config_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    new OutputConfigurationV2Interface(q, config_resource);
}

OutputManagementV2Interface::OutputManagementV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputManagementV2InterfacePrivate(this, display))
{
}

OutputManagementV2Interface::~OutputManagementV2Interface() = default;

}
