/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "drmleasedevice_v1_interface.h"
#include "drmleasedevice_v1_interface_p.h"
#include "display.h"
#include "logging.h"
#include "utils.h"

#include <fcntl.h>
#include <unistd.h>

namespace KWaylandServer
{

static const quint32 s_version = 1;

DrmLeaseDeviceV1Interface::DrmLeaseDeviceV1Interface(Display *display, std::function<int()> createNonMasterFd)
    : d(new DrmLeaseDeviceV1InterfacePrivate(display, this, createNonMasterFd))
{
}

DrmLeaseDeviceV1Interface::~DrmLeaseDeviceV1Interface()
{
    d->remove();
}

void DrmLeaseDeviceV1Interface::setDrmMaster(bool hasDrmMaster)
{
    if (hasDrmMaster && !d->hasDrmMaster) {
        // withdraw all connectors
        for (const auto &connector : qAsConst(d->connectors)) {
            DrmLeaseConnectorV1InterfacePrivate::get(connector)->withdraw();
        }
        // and revoke all leases
        for (const auto &lease : qAsConst(d->leases)) {
            lease->deny();
        }
    } else if (!hasDrmMaster && d->hasDrmMaster) {
        // send pending drm fds
        while (!d->pendingFds.isEmpty()) {
            int fd = d->createNonMasterFd();
            d->send_drm_fd(d->pendingFds.dequeue(), fd);
            close(fd);
        }
        // offer all connectors again
        for (const auto &connector : qAsConst(d->connectors)) {
            auto connectorPrivate = DrmLeaseConnectorV1InterfacePrivate::get(connector);
            connectorPrivate->withdrawn = false;
            for (const auto &resource : d->resourceMap()) {
                auto connectorResource = connectorPrivate->add(resource->client(), 0, s_version);
                d->send_connector(resource->handle, connectorResource->handle);
                connectorPrivate->send(connectorResource->handle);
            }
        }
    }
    d->hasDrmMaster = hasDrmMaster;
}


DrmLeaseDeviceV1InterfacePrivate::DrmLeaseDeviceV1InterfacePrivate(Display *display, DrmLeaseDeviceV1Interface *device, std::function<int()> createNonMasterFd)
    : QtWaylandServer::wp_drm_lease_device_v1(*display, s_version)
    , q(device)
    , createNonMasterFd(createNonMasterFd)
{
}

DrmLeaseDeviceV1InterfacePrivate::~DrmLeaseDeviceV1InterfacePrivate()
{
}

void DrmLeaseDeviceV1InterfacePrivate::remove()
{
    for (const auto &lease : qAsConst(leases)) {
        lease->deny();
    }
    for (const auto &connector : qAsConst(connectors)) {
        DrmLeaseConnectorV1InterfacePrivate::get(connector)->withdraw();
    }
    for (const auto &request : qAsConst(leaseRequests)) {
        request->connectors.clear();
    }
    globalRemove();
    removed = true;
    if (resourceMap().isEmpty()) {
        delete this;
    }
}

void DrmLeaseDeviceV1InterfacePrivate::registerConnector(DrmLeaseConnectorV1Interface *connector)
{
    connectors << connector;
    if (!hasDrmMaster) {
        return;
    }
    for (const auto &resource : resourceMap()) {
        auto connectorPrivate = DrmLeaseConnectorV1InterfacePrivate::get(connector);
        auto connectorResource = connectorPrivate->add(resource->client(), 0, resource->version());
        send_connector(resource->handle, connectorResource->handle);
        connectorPrivate->send(connectorResource->handle);
    }
}

void DrmLeaseDeviceV1InterfacePrivate::unregisterConnector(DrmLeaseConnectorV1Interface *connector)
{
    connectors.removeOne(connector);
    for (const auto &lease : qAsConst(leases)) {
        if (lease->d->connectors.contains(connector)) {
            lease->d->connectors.removeOne(connector);
            lease->deny();
        }
    }
    for (const auto &leaseRequest : qAsConst(leaseRequests)) {
        if (leaseRequest->connectors.removeOne(connector)) {
            leaseRequest->invalid = true;
        }
    }
}

DrmLeaseDeviceV1InterfacePrivate *DrmLeaseDeviceV1InterfacePrivate::get(DrmLeaseDeviceV1Interface *device)
{
    return device->d;
}

void DrmLeaseDeviceV1InterfacePrivate::wp_drm_lease_device_v1_create_lease_request(Resource *resource, uint32_t id)
{
    wl_resource *requestResource = wl_resource_create(resource->client(), &wp_drm_lease_request_v1_interface,
                                                      resource->version(), id);
    if (!requestResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    leaseRequests << new DrmLeaseRequestV1Interface(this, requestResource);
}

void DrmLeaseDeviceV1InterfacePrivate::wp_drm_lease_device_v1_release(Resource *resource)
{
    send_released(resource->handle);
    wl_resource_destroy(resource->handle);
}

void DrmLeaseDeviceV1InterfacePrivate::wp_drm_lease_device_v1_bind_resource(Resource *resource)
{
    if (!hasDrmMaster) {
        pendingFds << resource->handle;
        return;
    }
    int fd = createNonMasterFd();
    send_drm_fd(resource->handle, fd);
    close(fd);
    for (const auto &connector : qAsConst(connectors)) {
        auto connectorPrivate = DrmLeaseConnectorV1InterfacePrivate::get(connector);
        if (!connectorPrivate->withdrawn) {
            auto connectorResource = connectorPrivate->add(resource->client(), 0, s_version);
            send_connector(resource->handle, connectorResource->handle);
            connectorPrivate->send(connectorResource->handle);
        }
    }
}

void DrmLeaseDeviceV1InterfacePrivate::wp_drm_lease_device_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    if (removed && resourceMap().isEmpty()) {
        delete this;
    }
}


DrmLeaseConnectorV1Interface::DrmLeaseConnectorV1Interface(DrmLeaseDeviceV1Interface *leaseDevice,
                                                           uint32_t id,
                                                           const QString &name,
                                                           const QString &description)
    : d(new DrmLeaseConnectorV1InterfacePrivate(leaseDevice, this, id, name, description))
{
    DrmLeaseDeviceV1InterfacePrivate::get(leaseDevice)->registerConnector(this);
}

DrmLeaseConnectorV1Interface::~DrmLeaseConnectorV1Interface()
{
    d->withdraw();
    if (d->device) {
        auto devicePrivate = DrmLeaseDeviceV1InterfacePrivate::get(d->device);
        devicePrivate->unregisterConnector(this);
    }
}

DrmLeaseConnectorV1Interface *DrmLeaseConnectorV1Interface::get(wl_resource *resource)
{
    if (auto connectorPrivate = resource_cast<DrmLeaseConnectorV1InterfacePrivate*>(resource)) {
        return connectorPrivate->q;
    }
    return nullptr;
}

DrmLeaseConnectorV1InterfacePrivate::DrmLeaseConnectorV1InterfacePrivate(DrmLeaseDeviceV1Interface *device,
                                                                         DrmLeaseConnectorV1Interface *connector,
                                                                         uint32_t connectorId,
                                                                         const QString &name,
                                                                         const QString &description)
    : wp_drm_lease_connector_v1()
    , q(connector)
    , device(device)
    , connectorId(connectorId)
    , name(name)
    , description(description)
{
}

DrmLeaseConnectorV1InterfacePrivate::~DrmLeaseConnectorV1InterfacePrivate()
{
}

void DrmLeaseConnectorV1InterfacePrivate::send(wl_resource *resource)
{
    send_connector_id(resource, connectorId);
    send_name(resource, name);
    send_description(resource, description);
    send_done(resource);
}

void DrmLeaseConnectorV1InterfacePrivate::withdraw()
{
    if (!withdrawn) {
        withdrawn = true;
        for (const auto &resource : resourceMap()) {
            send_withdrawn(resource->handle);
            DrmLeaseDeviceV1InterfacePrivate::get(device)->send_done(resource->handle);
        }
    }
}

DrmLeaseConnectorV1InterfacePrivate *DrmLeaseConnectorV1InterfacePrivate::get(DrmLeaseConnectorV1Interface *connector)
{
    return connector->d.get();
}

void DrmLeaseConnectorV1InterfacePrivate::wp_drm_lease_connector_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}


DrmLeaseRequestV1Interface::DrmLeaseRequestV1Interface(DrmLeaseDeviceV1InterfacePrivate *device, wl_resource *resource)
    : wp_drm_lease_request_v1(resource)
    , device(device)
{
}

DrmLeaseRequestV1Interface::~DrmLeaseRequestV1Interface()
{
    device->leaseRequests.removeOne(this);
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_request_connector(Resource *resource, struct ::wl_resource *connector_handle)
{
    Q_UNUSED(resource);
    if (auto connector = DrmLeaseConnectorV1Interface::get(connector_handle)) {
        auto connectorPrivate = DrmLeaseConnectorV1InterfacePrivate::get(connector);
        if (connectorPrivate->device != device->q) {
            wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_WRONG_DEVICE, "Requested connector from invalid lease device");
        } else if (connectorPrivate->withdrawn) {
            qCWarning(KWAYLAND_SERVER) << "DrmLease: withdrawn connector requested";
        } else if (connectors.contains(connector)) {
            wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_DUPLICATE_CONNECTOR, "Requested connector twice");
        } else {
            connectors << connector;
        }
    } else {
        qCWarning(KWAYLAND_SERVER, "DrmLease: Invalid connector requested");
    }
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_submit(Resource *resource, uint32_t id)
{
    wl_resource *leaseResource = wl_resource_create(resource->client(), &wp_drm_lease_v1_interface, s_version, id);
    if (!leaseResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DrmLeaseV1Interface *lease = new DrmLeaseV1Interface(device, leaseResource);
    device->leases << lease;
    if (!device->hasDrmMaster) {
        qCWarning(KWAYLAND_SERVER) << "DrmLease: rejecting lease request without drm master";
        lease->deny();
    } else if (invalid) {
        qCWarning(KWAYLAND_SERVER()) << "DrmLease: rejecting lease request with a withdrawn connector";
        lease->deny();
    } else if (connectors.isEmpty()) {
        wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_EMPTY_LEASE, "Requested lease without connectors");
    } else {
        lease->d->connectors = connectors;
        emit device->q->leaseRequested(lease);
    }
    wl_resource_destroy(resource->handle);
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

DrmLeaseV1Interface::DrmLeaseV1Interface(DrmLeaseDeviceV1InterfacePrivate *device, wl_resource *resource)
    : d(new DrmLeaseV1InterfacePrivate(device, this, resource))
{
}

DrmLeaseV1Interface::~DrmLeaseV1Interface()
{
    deny();
    d->device->leases.removeOne(this);
}

void DrmLeaseV1Interface::grant(int leaseFd, uint32_t lesseeId)
{
    d->send_lease_fd(leaseFd);
    close(leaseFd);
    d->lesseeId = lesseeId;
    for (const auto &connector : qAsConst(d->connectors)) {
        DrmLeaseConnectorV1InterfacePrivate::get(connector)->withdraw();
    }
}

void DrmLeaseV1Interface::deny()
{
    if (!d->finished) {
        d->finished = true;
        d->send_finished();
    }
    if (!d->lesseeId) {
        return;
    }
    Q_EMIT d->device->q->leaseRevoked(this);
    // check if we should offer connectors again
    if (d->device->hasDrmMaster) {
        bool sent = false;
        for (const auto &connector : qAsConst(d->connectors)) {
            auto connectorPrivate = DrmLeaseConnectorV1InterfacePrivate::get(connector);
            connectorPrivate->withdrawn = false;
            for (const auto &resource : d->device->resourceMap()) {
                auto connectorResource = connectorPrivate->add(resource->client(), 0, s_version);
                d->device->send_connector(resource->handle, connectorResource->handle);
                connectorPrivate->send(connectorResource->handle);
                sent = true;
            }
        }
        if (sent) {
            for (const auto &resource : d->device->resourceMap()) {
                d->device->send_done(resource->handle);
            }
        }
    }
    d->lesseeId = 0;
}

uint32_t DrmLeaseV1Interface::lesseeId() const
{
    return d->lesseeId;
}

QVector<DrmLeaseConnectorV1Interface *> DrmLeaseV1Interface::connectors() const
{
    return d->connectors;
}


DrmLeaseV1InterfacePrivate::DrmLeaseV1InterfacePrivate(DrmLeaseDeviceV1InterfacePrivate *device, DrmLeaseV1Interface *q, wl_resource *resource)
    : wp_drm_lease_v1(resource)
    , device(device)
    , q(q)
{
}

void DrmLeaseV1InterfacePrivate::wp_drm_lease_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DrmLeaseV1InterfacePrivate::wp_drm_lease_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

}
