/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "utils/filedescriptor.h"
#include <qwayland-server-drm-lease-v1.h>

#include <QObject>
#include <QPointer>
#include <QQueue>

namespace KWaylandServer
{

class Display;
class DrmLeaseConnectorV1Interface;
class DrmLeaseRequestV1Interface;
class DrmLeaseV1Interface;

class DrmLeaseDeviceV1InterfacePrivate : public QtWaylandServer::wp_drm_lease_device_v1
{
public:
    DrmLeaseDeviceV1InterfacePrivate(Display *display, DrmLeaseDeviceV1Interface *device, std::function<KWin::FileDescriptor()> createNonMasterFd);
    ~DrmLeaseDeviceV1InterfacePrivate();
    void remove();

    void registerConnector(DrmLeaseConnectorV1Interface *connector);
    void unregisterConnector(DrmLeaseConnectorV1Interface *connector);

    static DrmLeaseDeviceV1InterfacePrivate *get(DrmLeaseDeviceV1Interface *device);

    DrmLeaseDeviceV1Interface *q;
    QVector<DrmLeaseConnectorV1Interface *> connectors;
    QVector<DrmLeaseRequestV1Interface *> leaseRequests;
    QVector<DrmLeaseV1Interface *> leases;
    QQueue<wl_resource *> pendingFds;
    std::function<KWin::FileDescriptor()> createNonMasterFd;
    bool hasDrmMaster = true;
    bool removed = false;

protected:
    void wp_drm_lease_device_v1_create_lease_request(Resource *resource, uint32_t id) override;
    void wp_drm_lease_device_v1_release(Resource *resource) override;
    void wp_drm_lease_device_v1_bind_resource(Resource *resource) override;
    void wp_drm_lease_device_v1_destroy_global() override;
};

class DrmLeaseConnectorV1InterfacePrivate : public QObject, public QtWaylandServer::wp_drm_lease_connector_v1
{
    Q_OBJECT
public:
    DrmLeaseConnectorV1InterfacePrivate(DrmLeaseDeviceV1Interface *device, DrmLeaseConnectorV1Interface *connector,
                                        uint32_t connectorId, const QString &name, const QString &description);
    ~DrmLeaseConnectorV1InterfacePrivate();

    void send(wl_resource *resource);
    void withdraw();

    static DrmLeaseConnectorV1InterfacePrivate *get(DrmLeaseConnectorV1Interface *connector);

    DrmLeaseConnectorV1Interface *q;
    QPointer<DrmLeaseDeviceV1Interface> device;
    uint32_t connectorId;
    QString name;
    QString description;
    bool withdrawn = false;

protected:
    void wp_drm_lease_connector_v1_destroy(Resource *resource) override;
};

class DrmLeaseRequestV1Interface : public QtWaylandServer::wp_drm_lease_request_v1
{
public:
    DrmLeaseRequestV1Interface(DrmLeaseDeviceV1InterfacePrivate *device, wl_resource *resource);
    ~DrmLeaseRequestV1Interface();

    DrmLeaseDeviceV1InterfacePrivate *device;
    QVector<DrmLeaseConnectorV1Interface *> connectors;
    bool invalid = false;

protected:
    void wp_drm_lease_request_v1_request_connector(Resource *resource, struct ::wl_resource *connector) override;
    void wp_drm_lease_request_v1_submit(Resource *resource, uint32_t id) override;
    void wp_drm_lease_request_v1_destroy_resource(Resource *resource) override;
};

class DrmLeaseV1InterfacePrivate : public QtWaylandServer::wp_drm_lease_v1
{
public:
    DrmLeaseV1InterfacePrivate(DrmLeaseDeviceV1InterfacePrivate *device, DrmLeaseV1Interface *q, wl_resource *resource);

    DrmLeaseDeviceV1InterfacePrivate *device;
    DrmLeaseV1Interface *q;
    QVector<DrmLeaseConnectorV1Interface *> connectors;
    uint32_t lesseeId = 0;
    bool finished = false;

protected:
    void wp_drm_lease_v1_destroy(Resource *resource) override;
    void wp_drm_lease_v1_destroy_resource(Resource *resource) override;
};

}
