/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgoutput_v1_interface.h"
#include "display.h"
#include "output_interface.h"

#include "qwayland-server-xdg-output-unstable-v1.h"

#include "core/output.h"

#include <QDebug>
#include <QHash>
#include <QPointer>

using namespace KWin;

namespace KWaylandServer
{
static const quint32 s_version = 3;

class XdgOutputManagerV1InterfacePrivate : public QtWaylandServer::zxdg_output_manager_v1
{
public:
    XdgOutputManagerV1InterfacePrivate(XdgOutputManagerV1Interface *q, Display *display);
    QHash<OutputInterface *, XdgOutputV1Interface *> outputs;

    XdgOutputManagerV1Interface *q;

protected:
    void zxdg_output_manager_v1_destroy(Resource *resource) override;
    void zxdg_output_manager_v1_get_xdg_output(Resource *resource, uint32_t id, wl_resource *output) override;
};

class XdgOutputV1InterfacePrivate : public QtWaylandServer::zxdg_output_v1
{
public:
    XdgOutputV1InterfacePrivate(XdgOutputV1Interface *q, OutputInterface *wlOutput)
        : output(wlOutput)
        , q(q)
    {
    }

    QPointF pos;
    QSizeF size;
    QString name;
    QString description;
    QPointer<OutputInterface> output;
    XdgOutputV1Interface *const q;

    void sendLogicalPosition(Resource *resource);
    void sendLogicalSize(Resource *resource);
    void sendDone(Resource *resource);

protected:
    void zxdg_output_v1_bind_resource(Resource *resource) override;
    void zxdg_output_v1_destroy(Resource *resource) override;
};

XdgOutputManagerV1Interface::XdgOutputManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgOutputManagerV1InterfacePrivate(this, display))
{
}

XdgOutputManagerV1Interface::~XdgOutputManagerV1Interface()
{
}

XdgOutputV1Interface *XdgOutputManagerV1Interface::createXdgOutput(OutputInterface *output, QObject *parent)
{
    Q_ASSERT_X(!d->outputs.contains(output), "createXdgOutput", "An XdgOuputInterface already exists for this output");

    auto xdgOutput = new XdgOutputV1Interface(output, parent);
    d->outputs[output] = xdgOutput;

    // as XdgOutput lifespan is managed by user, delete our mapping when either
    // it or the relevant Output gets deleted
    connect(output, &QObject::destroyed, this, [this, output]() {
        d->outputs.remove(output);
    });
    connect(xdgOutput, &QObject::destroyed, this, [this, output]() {
        d->outputs.remove(output);
    });

    return xdgOutput;
}

XdgOutputManagerV1InterfacePrivate::XdgOutputManagerV1InterfacePrivate(XdgOutputManagerV1Interface *qptr, Display *d)
    : QtWaylandServer::zxdg_output_manager_v1(*d, s_version)
    , q(qptr)
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
    xdgOutput->d->add(resource->client(), id, resource->version());
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

XdgOutputV1Interface::XdgOutputV1Interface(OutputInterface *output, QObject *parent)
    : QObject(parent)
    , d(new XdgOutputV1InterfacePrivate(this, output))
{
    const Output *handle = output->handle();

    d->name = handle->name();
    d->description = handle->description();
    d->pos = handle->geometry().topLeft();
    d->size = handle->geometry().size();

    connect(handle, &Output::geometryChanged, this, &XdgOutputV1Interface::update);
}

XdgOutputV1Interface::~XdgOutputV1Interface()
{
}

void XdgOutputV1Interface::update()
{
    if (!d->output || d->output->isRemoved()) {
        return;
    }

    const QRectF geometry = d->output->handle()->fractionalGeometry();
    const auto resources = d->resourceMap();

    if (d->pos != geometry.topLeft()) {
        d->pos = geometry.topLeft();
        for (auto resource : resources) {
            d->sendLogicalPosition(resource);
        }
    }

    if (d->size != geometry.size()) {
        d->size = geometry.size();
        for (auto resource : resources) {
            d->sendLogicalSize(resource);
        }
    }

    for (auto resource : resources) {
        if (wl_resource_get_version(resource->handle) < 3) {
            d->send_done(resource->handle);
        }
    }

    d->output->scheduleDone();
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_bind_resource(Resource *resource)
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
    QObject::connect(connection, &ClientConnection::scaleOverrideChanged, q, &XdgOutputV1Interface::resend, Qt::UniqueConnection);
}

void XdgOutputV1InterfacePrivate::sendLogicalSize(Resource *resource)
{
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();

    send_logical_size(resource->handle, std::round(size.width() * scaleOverride), std::round(size.height() * scaleOverride));
}

void XdgOutputV1InterfacePrivate::sendLogicalPosition(Resource *resource)
{
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();

    send_logical_position(resource->handle, pos.x() * scaleOverride, pos.y() * scaleOverride);
}

void XdgOutputV1InterfacePrivate::sendDone(Resource *resource)
{
    if (wl_resource_get_version(resource->handle) >= 3) {
        output->done(resource->client());
    } else {
        send_done(resource->handle);
    }
}

void XdgOutputV1Interface::resend()
{
    if (!d->output || d->output->isRemoved()) {
        return;
    }

    auto changedConnection = qobject_cast<ClientConnection *>(sender());
    const auto outputResources = d->resourceMap();
    for (auto resource : outputResources) {
        ClientConnection *connection = d->output->display()->getConnection(resource->client());
        if (connection == changedConnection) {
            d->sendLogicalPosition(resource);
            d->sendLogicalSize(resource);
            d->sendDone(resource);
        }
    }
}
}
