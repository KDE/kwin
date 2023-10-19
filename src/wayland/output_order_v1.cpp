/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "output_order_v1.h"
#include "core/output.h"
#include "display.h"

#include "qwayland-server-kde-output-order-v1.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

class OutputOrderV1InterfacePrivate : public QtWaylandServer::kde_output_order_v1
{
public:
    OutputOrderV1InterfacePrivate(Display *display);

    void sendList(wl_resource *resource);
    QList<Output *> outputOrder;

protected:
    void kde_output_order_v1_bind_resource(Resource *resource) override;
    void kde_output_order_v1_destroy(Resource *resource) override;
};

OutputOrderV1Interface::OutputOrderV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<OutputOrderV1InterfacePrivate>(display))
{
}

OutputOrderV1Interface::~OutputOrderV1Interface() = default;

void OutputOrderV1Interface::setOutputOrder(const QList<Output *> &outputOrder)
{
    d->outputOrder = outputOrder;
    const auto resources = d->resourceMap();
    for (const auto &resource : resources) {
        d->sendList(resource->handle);
    }
}

OutputOrderV1InterfacePrivate::OutputOrderV1InterfacePrivate(Display *display)
    : QtWaylandServer::kde_output_order_v1(*display, s_version)
{
}

void OutputOrderV1InterfacePrivate::kde_output_order_v1_bind_resource(Resource *resource)
{
    sendList(resource->handle);
}

void OutputOrderV1InterfacePrivate::sendList(wl_resource *resource)
{
    for (Output *const output : std::as_const(outputOrder)) {
        kde_output_order_v1_send_output(resource, output->name().toUtf8().constData());
    }
    kde_output_order_v1_send_done(resource);
}

void OutputOrderV1InterfacePrivate::kde_output_order_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}
}

#include "moc_output_order_v1.cpp"
