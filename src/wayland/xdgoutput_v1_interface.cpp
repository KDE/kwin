/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgoutput_v1_interface.h"
#include "display.h"
#include "output_interface.h"

#include "wayland-xdg-output-unstable-v1-server-protocol.h"

#include <QDebug>
#include <QHash>
#include <QPointer>

namespace KWaylandServer
{

static const quint32 s_version = 3;

class XdgOutputManagerV1InterfacePrivate
{
public:
    explicit XdgOutputManagerV1InterfacePrivate(Display *display);
    ~XdgOutputManagerV1InterfacePrivate();

    static XdgOutputManagerV1InterfacePrivate *fromResource(wl_resource *resource);

    QHash<OutputInterface *, XdgOutputV1Interface *> outputs;
    wl_global *global = nullptr;

    static void zxdg_output_manager_v1_bind_resource(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void zxdg_output_manager_v1_destroy(wl_client *client, wl_resource *resource);
    static void zxdg_output_manager_v1_get_xdg_output(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output);

    static constexpr struct zxdg_output_manager_v1_interface implementation = {
        .destroy = zxdg_output_manager_v1_destroy,
        .get_xdg_output = zxdg_output_manager_v1_get_xdg_output,
    };
};

class XdgOutputV1InterfacePrivate
{
public:
    XdgOutputV1InterfacePrivate(XdgOutputV1Interface *q, OutputInterface *output);
    ~XdgOutputV1InterfacePrivate();

    static XdgOutputV1InterfacePrivate *fromResource(wl_resource *resource);

    QMultiMap<wl_client *, wl_resource *> resourceMap;
    QPoint pos;
    QSize size;
    QString name;
    QString description;
    bool dirty = false;
    bool doneOnce = false;
    QPointer<OutputInterface> output;
    XdgOutputV1Interface *const q;

    void add(wl_client *client, wl_resource *resource);

    void sendLogicalPosition(wl_resource *resource, const QPoint &position);
    void sendLogicalSize(wl_resource *resource, const QSize &size);
    void sendDone(wl_resource *resource);

    static void zxdg_output_v1_destroy_resource(wl_resource *resource);
    static void zxdg_output_v1_destroy(wl_client *client, wl_resource *resource);

    static constexpr struct zxdg_output_v1_interface implementation = {
        .destroy = zxdg_output_v1_destroy,
    };
};

XdgOutputManagerV1InterfacePrivate *XdgOutputManagerV1InterfacePrivate::fromResource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &zxdg_output_manager_v1_interface, &implementation));
    return static_cast<XdgOutputManagerV1InterfacePrivate *>(wl_resource_get_user_data(resource));
}

XdgOutputManagerV1InterfacePrivate::XdgOutputManagerV1InterfacePrivate(Display *display)
{
    global = wl_global_create(*display, &zxdg_output_manager_v1_interface, s_version, this, zxdg_output_manager_v1_bind_resource);
}

XdgOutputManagerV1InterfacePrivate::~XdgOutputManagerV1InterfacePrivate()
{
    if (global) {
        wl_global_destroy(global);
        global = nullptr;
    }
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_bind_resource(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &zxdg_output_manager_v1_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &implementation, data, nullptr);
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_get_xdg_output(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output_resource)
{
    XdgOutputManagerV1InterfacePrivate *managerPrivate = XdgOutputManagerV1InterfacePrivate::fromResource(resource);

    wl_resource *xdgOutputResource = wl_resource_create(client, &zxdg_output_v1_interface, wl_resource_get_version(resource), id);
    if (!xdgOutputResource) {
        wl_client_post_no_memory(client);
        return;
    }

    XdgOutputV1Interface *xdgOutput = managerPrivate->outputs.value(OutputInterface::get(output_resource));

    // means that the output has been removed
    if (!xdgOutput) {
        wl_resource_set_implementation(xdgOutputResource,
                                       &XdgOutputV1InterfacePrivate::implementation,
                                       nullptr,
                                       nullptr);
        return;
    }

    wl_resource_set_implementation(xdgOutputResource,
                                   &XdgOutputV1InterfacePrivate::implementation,
                                   xdgOutput->d.get(),
                                   XdgOutputV1InterfacePrivate::zxdg_output_v1_destroy_resource);

    xdgOutput->d->add(client, xdgOutputResource);
}

void XdgOutputManagerV1InterfacePrivate::zxdg_output_manager_v1_destroy(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

XdgOutputManagerV1Interface::XdgOutputManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgOutputManagerV1InterfacePrivate(display))
{
    connect(display, &Display::aboutToTerminate, this, [this]() {
        wl_global_destroy(d->global);
        d->global = nullptr;
    });
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

XdgOutputV1Interface::XdgOutputV1Interface(OutputInterface *output, QObject *parent)
    : QObject(parent)
    , d(new XdgOutputV1InterfacePrivate(this, output))
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

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
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

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
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

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
        if (wl_resource_get_version(resource) < 3) {
            zxdg_output_v1_send_done(resource);
        }
    }
}

XdgOutputV1InterfacePrivate *XdgOutputV1InterfacePrivate::fromResource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &zxdg_output_v1_interface, &implementation));
    return static_cast<XdgOutputV1InterfacePrivate *>(wl_resource_get_user_data(resource));
}

XdgOutputV1InterfacePrivate::XdgOutputV1InterfacePrivate(XdgOutputV1Interface *q, OutputInterface *output)
    : output(output)
    , q(q)
{
}

XdgOutputV1InterfacePrivate::~XdgOutputV1InterfacePrivate()
{
    for (wl_resource *resource : std::as_const(resourceMap)) {
        wl_resource_set_user_data(resource, nullptr);
    }
}

void XdgOutputV1InterfacePrivate::add(wl_client *client, wl_resource *resource)
{
    resourceMap.insert(client, resource);

    if (wl_resource_get_version(resource) >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
        zxdg_output_v1_send_name(resource, name.toUtf8());
    }
    if (wl_resource_get_version(resource) >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
        zxdg_output_v1_send_description(resource, description.toUtf8());
    }

    sendLogicalPosition(resource, pos);
    sendLogicalSize(resource, size);
    sendDone(resource);

    ClientConnection *connection = output->display()->getConnection(wl_resource_get_client(resource));
    QObject::connect(connection, &ClientConnection::scaleOverrideChanged, q, &XdgOutputV1Interface::sendRefresh, Qt::UniqueConnection);
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_destroy_resource(wl_resource *resource)
{
    auto outputPrivate = XdgOutputV1InterfacePrivate::fromResource(resource);
    if (outputPrivate) {
        outputPrivate->resourceMap.remove(wl_resource_get_client(resource), resource);
    }
}

void XdgOutputV1InterfacePrivate::zxdg_output_v1_destroy(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void XdgOutputV1InterfacePrivate::sendLogicalSize(wl_resource *resource, const QSize &size)
{
    ClientConnection *connection = output->display()->getConnection(wl_resource_get_client(resource));
    qreal scaleOverride = connection->scaleOverride();

    zxdg_output_v1_send_logical_size(resource, size.width() * scaleOverride, size.height() * scaleOverride);
}

void XdgOutputV1InterfacePrivate::sendLogicalPosition(wl_resource *resource, const QPoint &pos)
{
    ClientConnection *connection = output->display()->getConnection(wl_resource_get_client(resource));
    qreal scaleOverride = connection->scaleOverride();

    zxdg_output_v1_send_logical_position(resource, pos.x() * scaleOverride, pos.y() * scaleOverride);
}

void XdgOutputV1InterfacePrivate::sendDone(wl_resource *resource)
{
    if (!doneOnce) {
        return;
    }

    if (wl_resource_get_version(resource) >= 3) {
        if (output) {
            output->done(wl_resource_get_client(resource));
        }
    } else {
        zxdg_output_v1_send_done(resource);
    }
}

void XdgOutputV1Interface::sendRefresh()
{
    auto changedConnection = qobject_cast<ClientConnection *>(sender());

    for (wl_resource *resource : std::as_const(d->resourceMap)) {
        ClientConnection *connection = d->output->display()->getConnection(wl_resource_get_client(resource));
        if (connection == changedConnection) {
            d->sendLogicalPosition(resource, d->pos);
            d->sendLogicalSize(resource, d->size);
            d->sendDone(resource);
        }
    }
}
}
