/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgoutput_v1_interface.h"
#include "display.h"
#include "output_interface.h"

#include "qwayland-server-xdg-output-unstable-v1.h"

#include <QDebug>
#include <QHash>
#include <QPointer>

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
    XdgOutputV1InterfacePrivate(OutputInterface *wlOutput)
        : output(wlOutput)
    {
    }

    QPoint pos;
    QSize size;
    QString name;
    QString description;
    bool dirty = false;
    bool doneOnce = false;
    QPointer<OutputInterface> output;

    void sendLogicalPosition(Resource *resource, const QPoint &position);
    void sendLogicalSize(Resource *resource, const QSize &size);

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
    , d(new XdgOutputV1InterfacePrivate(output))
{
}

XdgOutputV1Interface::~XdgOutputV1Interface()
{
}

void XdgOutputV1Interface::setLogicalSize(const QSize &size)
{
    if (size == d->size) {
        return;
    }
    d->size = size;
    d->dirty = true;

    const auto outputResources = d->resourceMap();
    for (auto resource : outputResources) {
        d->sendLogicalSize(resource, size);
    }
}

QSize XdgOutputV1Interface::logicalSize() const
{
    return d->size;
}

void XdgOutputV1Interface::setLogicalPosition(const QPoint &pos)
{
    if (pos == d->pos) {
        return;
    }
    d->pos = pos;
    d->dirty = true;

    const auto outputResources = d->resourceMap();
    for (auto resource : outputResources) {
        d->sendLogicalPosition(resource, pos);
    }
}

QPoint XdgOutputV1Interface::logicalPosition() const
{
    return d->pos;
}

void XdgOutputV1Interface::setName(const QString &name)
{
    d->name = name;
    // this can only be set once before the client connects
}

void XdgOutputV1Interface::setDescription(const QString &description)
{
    d->description = description;
    // this can only be set once before the client connects
}

void XdgOutputV1Interface::done()
{
    d->doneOnce = true;
    if (!d->dirty) {
        return;
    }
    d->dirty = false;

    const auto outputResources = d->resourceMap();
    for (auto resource : outputResources) {
        if (wl_resource_get_version(resource->handle) < 3) {
            d->send_done(resource->handle);
        }
    }
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_bind_resource(Resource *resource)
{
    sendLogicalPosition(resource, pos);
    sendLogicalSize(resource, size);
    if (resource->version() >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
        send_name(resource->handle, name);
    }
    if (resource->version() >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
        send_description(resource->handle, description);
    }

    if (doneOnce) {
        if (wl_resource_get_version(resource->handle) >= 3) {
            if (output) {
                output->done(resource->client());
            }
        } else {
            send_done(resource->handle);
        }
    }
}

void XdgOutputV1InterfacePrivate::sendLogicalSize(Resource *resource, const QSize &size)
{
    if (!output) {
        return;
    }
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();

    send_logical_size(resource->handle, size.width() * scaleOverride, size.height() * scaleOverride);
}

void XdgOutputV1InterfacePrivate::sendLogicalPosition(Resource *resource, const QPoint &pos)
{
    if (!output) {
        return;
    }
    ClientConnection *connection = output->display()->getConnection(resource->client());
    qreal scaleOverride = connection->scaleOverride();
    send_logical_position(resource->handle, pos.x() * scaleOverride, pos.y() * scaleOverride);
}
}
