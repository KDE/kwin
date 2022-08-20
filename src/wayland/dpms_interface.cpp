/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "dpms_interface_p.h"
#include "output_interface.h"

using namespace KWin;

namespace KWaylandServer
{
static const quint32 s_version = 1;

DpmsManagerInterfacePrivate::DpmsManagerInterfacePrivate(DpmsManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_dpms_manager(*display, s_version)
    , q(_q)
{
}

void DpmsManagerInterfacePrivate::org_kde_kwin_dpms_manager_get(Resource *resource, uint32_t id, wl_resource *output)
{
    OutputInterface *o = OutputInterface::get(output);

    wl_resource *dpms_resource = wl_resource_create(resource->client(), &org_kde_kwin_dpms_interface, resource->version(), id);
    if (!dpms_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    new DpmsInterface(o, dpms_resource);
}

DpmsManagerInterface::DpmsManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new DpmsManagerInterfacePrivate(this, display))
{
}

DpmsManagerInterface::~DpmsManagerInterface() = default;

DpmsInterface::DpmsInterface(OutputInterface *output, wl_resource *resource)
    : QObject()
    , QtWaylandServer::org_kde_kwin_dpms(resource)
    , m_output(output)
{
    if (!m_output) {
        return;
    }

    sendSupported();
    sendMode();
    sendDone();

    connect(m_output->handle(), &Output::capabilitiesChanged, this, [this]() {
        sendSupported();
        sendDone();
    });
    connect(m_output->handle(), &Output::dpmsModeChanged, this, [this]() {
        sendMode();
        sendDone();
    });
}

void DpmsInterface::org_kde_kwin_dpms_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DpmsInterface::org_kde_kwin_dpms_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void DpmsInterface::org_kde_kwin_dpms_set(Resource *resource, uint32_t mode)
{
    Q_UNUSED(resource)
    if (!m_output) {
        return;
    }

    Output::DpmsMode dpmsMode;
    switch (mode) {
    case ORG_KDE_KWIN_DPMS_MODE_ON:
        dpmsMode = Output::DpmsMode::On;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_STANDBY:
        dpmsMode = Output::DpmsMode::Standby;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_SUSPEND:
        dpmsMode = Output::DpmsMode::Suspend;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_OFF:
        dpmsMode = Output::DpmsMode::Off;
        break;
    default:
        return;
    }

    m_output->handle()->setDpmsMode(dpmsMode);
}

void DpmsInterface::sendSupported()
{
    send_supported(m_output->handle()->capabilities() & Output::Capability::Dpms ? 1 : 0);
}

void DpmsInterface::sendMode()
{
    const auto mode = m_output->handle()->dpmsMode();
    org_kde_kwin_dpms_mode wlMode;
    switch (mode) {
    case KWin::Output::DpmsMode::On:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_ON;
        break;
    case KWin::Output::DpmsMode::Standby:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_STANDBY;
        break;
    case KWin::Output::DpmsMode::Suspend:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_SUSPEND;
        break;
    case KWin::Output::DpmsMode::Off:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_OFF;
        break;
    default:
        Q_UNREACHABLE();
    }
    send_mode(wlMode);
}

void DpmsInterface::sendDone()
{
    send_done();
}

}
