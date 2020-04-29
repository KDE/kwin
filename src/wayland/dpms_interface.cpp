/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "dpms_interface_p.h"
#include "display.h"
#include "output_interface.h"

namespace KWaylandServer
{

const quint32 DpmsManagerInterface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct org_kde_kwin_dpms_manager_interface DpmsManagerInterface::Private::s_interface = {
    getDpmsCallback
};
#endif

DpmsManagerInterface::Private::Private(DpmsManagerInterface *qptr, Display *d)
    : Global::Private(d, &org_kde_kwin_dpms_manager_interface, s_version)
    , q(qptr)
{
}

void DpmsManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *dpms = c->createResource(&org_kde_kwin_dpms_manager_interface, qMin(version, s_version), id);
    if (!dpms) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(dpms, &s_interface, this, nullptr);
}

void DpmsManagerInterface::Private::getDpmsCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output)
{
    auto p = Private::cast(resource);
    auto c = p->display->getConnection(client);
    OutputInterface *o = OutputInterface::get(output);
    DpmsInterface *dpms = new DpmsInterface(o, resource, p->q);
    dpms->create(c, wl_resource_get_version(resource), id);
    if (!dpms->resource()) {
        wl_resource_post_no_memory(resource);
        return;
    }
    dpms->sendSupported();
    dpms->sendMode();
    dpms->sendDone();
}

DpmsManagerInterface::DpmsManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

DpmsManagerInterface::~DpmsManagerInterface() = default;


#ifndef K_DOXYGEN
const struct org_kde_kwin_dpms_interface DpmsInterface::Private::s_interface = {
    setCallback,
    resourceDestroyedCallback
};
#endif

DpmsInterface::Private::Private(DpmsInterface *q, DpmsManagerInterface *g, wl_resource *parentResource, OutputInterface *outputInterface)
    : Resource::Private(q, g, parentResource, &org_kde_kwin_dpms_interface, &s_interface)
    , output(outputInterface)
{
}

void DpmsInterface::Private::setCallback(wl_client *client, wl_resource *resource, uint32_t mode)
{
    Q_UNUSED(client)
    OutputInterface::DpmsMode dpmsMode;
    switch (mode) {
    case ORG_KDE_KWIN_DPMS_MODE_ON:
        dpmsMode = OutputInterface::DpmsMode::On;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_STANDBY:
        dpmsMode = OutputInterface::DpmsMode::Standby;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_SUSPEND:
        dpmsMode = OutputInterface::DpmsMode::Suspend;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_OFF:
        dpmsMode = OutputInterface::DpmsMode::Off;
        break;
    default:
        return;
    }
    emit cast<Private>(resource)->output->dpmsModeRequested(dpmsMode);
}

DpmsInterface::DpmsInterface(OutputInterface *output, wl_resource *parentResource, DpmsManagerInterface *manager)
    : Resource(new Private(this, manager, parentResource, output))
{
    connect(output, &OutputInterface::dpmsSupportedChanged, this,
        [this] {
            sendSupported();
            sendDone();
        }
    );
    connect(output, &OutputInterface::dpmsModeChanged, this,
        [this] {
            sendMode();
            sendDone();
        }
    );
}

DpmsInterface::~DpmsInterface() = default;

void DpmsInterface::sendSupported()
{
    Q_D();
    org_kde_kwin_dpms_send_supported(d->resource, d->output->isDpmsSupported() ? 1 : 0);
}

void DpmsInterface::sendMode()
{
    Q_D();
    const auto mode = d->output->dpmsMode();
    org_kde_kwin_dpms_mode wlMode;
    switch (mode) {
    case OutputInterface::DpmsMode::On:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_ON;
        break;
    case OutputInterface::DpmsMode::Standby:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_STANDBY;
        break;
    case OutputInterface::DpmsMode::Suspend:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_SUSPEND;
        break;
    case OutputInterface::DpmsMode::Off:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_OFF;
        break;
    default:
        Q_UNREACHABLE();
    }
    org_kde_kwin_dpms_send_mode(d->resource, wlMode);
}

void DpmsInterface::sendDone()
{
    Q_D();
    org_kde_kwin_dpms_send_done(d->resource);
    client()->flush();
}

DpmsInterface::Private *DpmsInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
