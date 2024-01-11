/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgoutput_v1.h"
#include "clientconnection.h"
#include "display.h"
#include "output.h"

#include "qwayland-server-xdg-output-unstable-v1.h"

#include "core/output.h"

#include <QDebug>
#include <QHash>
#include <QPointer>
#include <cmath>

namespace KWin
{
static const quint32 s_version = 3;

class XdgOutputV1Interface : public QObject, public QtWaylandServer::zxdg_output_v1
{
public:
    explicit XdgOutputV1Interface(OutputInterface *wlOutput);

    void resend();
    void update();

    QPointF pos;
    QSizeF size;
    QString name;
    QString description;
    QPointer<OutputInterface> output;

    void sendLogicalPosition(Resource *resource);
    void sendLogicalSize(Resource *resource);
    void sendDone(Resource *resource);

protected:
    void zxdg_output_v1_bind_resource(Resource *resource) override;
    void zxdg_output_v1_destroy(Resource *resource) override;
};

class XdgOutputManagerV1InterfacePrivate : public QtWaylandServer::zxdg_output_manager_v1
{
public:
    explicit XdgOutputManagerV1InterfacePrivate(Display *display);

    QHash<OutputInterface *, XdgOutputV1Interface *> outputs;

protected:
    void zxdg_output_manager_v1_destroy(Resource *resource) override;
    void zxdg_output_manager_v1_get_xdg_output(Resource *resource, uint32_t id, wl_resource *output) override;
};

XdgOutputManagerV1Interface::XdgOutputManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgOutputManagerV1InterfacePrivate(display))
{
}

XdgOutputManagerV1Interface::~XdgOutputManagerV1Interface()
{
}

void XdgOutputManagerV1Interface::offer(OutputInterface *output)
{
    Q_ASSERT_X(!d->outputs.contains(output), "offer", "An XdgOuputInterface already exists for this output");

    auto xdgOutput = new XdgOutputV1Interface(output);
    d->outputs[output] = xdgOutput;
    connect(output, &QObject::destroyed, this, [this, output]() {
        delete d->outputs.take(output);
    });
}

XdgOutputManagerV1InterfacePrivate::XdgOutputManagerV1InterfacePrivate(Display *d)
    : QtWaylandServer::zxdg_output_manager_v1(*d, s_version)
{
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_get_xdg_output(Resource *resource, uint32_t id, wl_resource *outputResource)
{
    auto output = OutputInterface::get(outputResource);
    if (!output) { // output client is requesting XdgOutput for an Output that doesn't exist
        return;
    }
    auto xdgOutput = outputs.value(output);
    if (!xdgOutput) {
        return; // client is requesting XdgOutput for an Output that doesn't exist
    }
    xdgOutput->add(resource->client(), id, resource->version());
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

XdgOutputV1Interface::XdgOutputV1Interface(OutputInterface *output)
    : output(output)
{
    const Output *handle = output->handle();

    name = handle->name();
    description = handle->description();
    pos = handle->geometryF().topLeft();
    size = handle->geometryF().size();

    connect(handle, &Output::geometryChanged, this, &XdgOutputV1Interface::update);
}

void XdgOutputV1Interface::update()
{
    if (!output || output->isRemoved()) {
        return;
    }

    const QRectF geometry = output->handle()->geometryF();
    const auto resources = resourceMap();

    if (pos != geometry.topLeft()) {
        pos = geometry.topLeft();
        for (auto resource : resources) {
            sendLogicalPosition(resource);
        }
    }

    if (size != geometry.size()) {
        size = geometry.size();
        for (auto resource : resources) {
            sendLogicalSize(resource);
        }
    }

    for (auto resource : resources) {
        if (wl_resource_get_version(resource->handle) < 3) {
            send_done(resource->handle);
        }
    }

    output->scheduleDone();
}

void XdgOutputV1Interface::zxdg_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgOutputV1Interface::zxdg_output_v1_bind_resource(Resource *resource)
{
    if (!output || output->isRemoved()) {
        return;
    }

    sendLogicalPosition(resource);
    sendLogicalSize(resource);
    if (resource->version() >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
        send_name(resource->handle, name);
    }
    if (resource->version() >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
        send_description(resource->handle, description);
    }

    sendDone(resource);

    ClientConnection *connection = output->display()->getConnection(resource->client());
    connect(connection, &ClientConnection::scaleOverrideChanged, this, &XdgOutputV1Interface::resend, Qt::UniqueConnection);
}

void XdgOutputV1Interface::sendLogicalSize(Resource *resource)
{
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();

    send_logical_size(resource->handle, std::round(size.width() * scaleOverride), std::round(size.height() * scaleOverride));
}

void XdgOutputV1Interface::sendLogicalPosition(Resource *resource)
{
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();

    send_logical_position(resource->handle, pos.x() * scaleOverride, pos.y() * scaleOverride);
}

void XdgOutputV1Interface::sendDone(Resource *resource)
{
    if (wl_resource_get_version(resource->handle) >= 3) {
        output->done(resource->client());
    } else {
        send_done(resource->handle);
    }
}

void XdgOutputV1Interface::resend()
{
    if (!output || output->isRemoved()) {
        return;
    }

    auto changedConnection = qobject_cast<ClientConnection *>(sender());
    const auto outputResources = resourceMap();
    for (auto resource : outputResources) {
        ClientConnection *connection = output->display()->getConnection(resource->client());
        if (connection == changedConnection) {
            sendLogicalPosition(resource);
            sendLogicalSize(resource);
            sendDone(resource);
        }
    }
}
}

#include "moc_xdgoutput_v1.cpp"
