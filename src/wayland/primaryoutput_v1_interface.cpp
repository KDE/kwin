/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "primaryoutput_v1_interface.h"

#include "display.h"
#include "outputconfiguration_v2_interface.h"
#include "outputmanagement_v2_interface.h"

#include "qwayland-server-kde-primary-output-v1.h"
#include <wayland-server.h>

#include <QHash>

namespace KWaylandServer
{
static const quint32 s_version = 1;

class PrimaryOutputV1InterfacePrivate : public QtWaylandServer::kde_primary_output_v1
{
public:
    PrimaryOutputV1InterfacePrivate(Display *display)
        : QtWaylandServer::kde_primary_output_v1(*display, s_version)
    {
    }

    void kde_primary_output_v1_bind_resource(Resource *resource) override
    {
        if (!m_outputName.isEmpty()) {
            send_primary_output(resource->handle, m_outputName);
        }
    }

    QString m_outputName;
};

PrimaryOutputV1Interface::PrimaryOutputV1Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , d(new PrimaryOutputV1InterfacePrivate(display))
{
}

PrimaryOutputV1Interface::~PrimaryOutputV1Interface() = default;

void PrimaryOutputV1Interface::setPrimaryOutput(const QString &outputName)
{
    if (outputName == d->m_outputName) {
        return;
    }
    d->m_outputName = outputName;

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_primary_output(resource->handle, outputName);
    }
}

}
