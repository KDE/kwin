/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "dpms.h"
#include "core/backendoutput.h"
#include "display.h"
#include "output.h"

#include <QPointer>

#include <qwayland-server-dpms.h>

namespace KWin
{

static const quint32 s_version = 1;

class DpmsManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_dpms_manager
{
public:
    DpmsManagerInterfacePrivate(Display *d);

protected:
    void org_kde_kwin_dpms_manager_get(Resource *resource, uint32_t id, wl_resource *output) override;
};

class DpmsInterface : public QObject, QtWaylandServer::org_kde_kwin_dpms
{
    Q_OBJECT
public:
    explicit DpmsInterface(OutputInterface *output, wl_resource *resource);

    void sendSupported();
    void sendMode();
    void sendDone();

    QPointer<OutputInterface> m_output;

protected:
    void org_kde_kwin_dpms_destroy_resource(Resource *resource) override;
    void org_kde_kwin_dpms_set(Resource *resource, uint32_t mode) override;
    void org_kde_kwin_dpms_release(Resource *resource) override;
};

DpmsManagerInterfacePrivate::DpmsManagerInterfacePrivate(Display *display)
    : QtWaylandServer::org_kde_kwin_dpms_manager(*display, s_version)
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
    , d(new DpmsManagerInterfacePrivate(display))
{
}

DpmsManagerInterface::~DpmsManagerInterface() = default;

DpmsInterface::DpmsInterface(OutputInterface *output, wl_resource *resource)
    : QObject()
    , QtWaylandServer::org_kde_kwin_dpms(resource)
    , m_output(output)
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    sendSupported();
    sendMode();
    sendDone();

    connect(m_output->handle()->backendOutput(), &BackendOutput::capabilitiesChanged, this, [this]() {
        sendSupported();
        sendDone();
    });
    connect(m_output->handle()->backendOutput(), &BackendOutput::dpmsModeChanged, this, [this]() {
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
    delete this;
}

void DpmsInterface::org_kde_kwin_dpms_set(Resource *resource, uint32_t mode)
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    BackendOutput::DpmsMode dpmsMode;
    switch (mode) {
    case ORG_KDE_KWIN_DPMS_MODE_ON:
        dpmsMode = BackendOutput::DpmsMode::On;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_STANDBY:
        dpmsMode = BackendOutput::DpmsMode::Standby;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_SUSPEND:
        dpmsMode = BackendOutput::DpmsMode::Suspend;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_OFF:
        dpmsMode = BackendOutput::DpmsMode::Off;
        break;
    default:
        return;
    }

    m_output->handle()->backendOutput()->setDpmsMode(dpmsMode);
}

void DpmsInterface::sendSupported()
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    send_supported(m_output->handle()->backendOutput()->capabilities() & BackendOutput::Capability::Dpms ? 1 : 0);
}

void DpmsInterface::sendMode()
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    const auto mode = m_output->handle()->backendOutput()->dpmsMode();
    org_kde_kwin_dpms_mode wlMode;
    switch (mode) {
    case BackendOutput::DpmsMode::On:
    case BackendOutput::DpmsMode::AboutToTurnOff:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_ON;
        break;
    case BackendOutput::DpmsMode::Standby:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_STANDBY;
        break;
    case BackendOutput::DpmsMode::Suspend:
        wlMode = ORG_KDE_KWIN_DPMS_MODE_SUSPEND;
        break;
    case BackendOutput::DpmsMode::Off:
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

#include "dpms.moc"

#include "moc_dpms.cpp"
