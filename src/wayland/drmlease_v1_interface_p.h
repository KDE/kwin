/*
    SPDX-FileCopyrightText: 2021-2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <qwayland-server-drm-lease-v1.h>

#include "backends/drm/drm_backend.h"
#include "backends/drm/drm_connector.h"
#include "backends/drm/drm_gpu.h"
#include "backends/drm/drm_output.h"
#include "utils/filedescriptor.h"

#include <QObject>
#include <QPointer>
#include <QQueue>

namespace KWaylandServer
{

class Display;
class DrmLeaseConnectorV1Interface;
class DrmLeaseRequestV1Interface;
class DrmLeaseV1Interface;

class DrmLeaseDeviceV1Interface : public QObject, public QtWaylandServer::wp_drm_lease_device_v1
{
public:
    explicit DrmLeaseDeviceV1Interface(Display *display, KWin::DrmGpu *gpu);
    ~DrmLeaseDeviceV1Interface();

    void addOutput(KWin::DrmAbstractOutput *output);
    void removeOutput(KWin::DrmAbstractOutput *output);
    void setDrmMaster(bool hasDrmMaster);
    void done();
    void remove();
    void addLeaseRequest(DrmLeaseRequestV1Interface *leaseRequest);
    void removeLeaseRequest(DrmLeaseRequestV1Interface *leaseRequest);
    void addLease(DrmLeaseV1Interface *lease);
    void removeLease(DrmLeaseV1Interface *lease);
    void offerConnector(DrmLeaseConnectorV1Interface *connector);

    bool hasDrmMaster() const;
    KWin::DrmGpu *gpu() const;

private:
    void wp_drm_lease_device_v1_create_lease_request(Resource *resource, uint32_t id) override;
    void wp_drm_lease_device_v1_release(Resource *resource) override;
    void wp_drm_lease_device_v1_bind_resource(Resource *resource) override;
    void wp_drm_lease_device_v1_destroy_global() override;

    KWin::DrmGpu *const m_gpu;
    bool m_hasDrmMaster = true;
    std::map<KWin::DrmAbstractOutput *, std::unique_ptr<DrmLeaseConnectorV1Interface>> m_connectors;
    QQueue<wl_resource *> m_pendingFds;
    QVector<DrmLeaseRequestV1Interface *> m_leaseRequests;
    QVector<DrmLeaseV1Interface *> m_leases;
};

class DrmLeaseConnectorV1Interface : public QObject, public QtWaylandServer::wp_drm_lease_connector_v1
{
    Q_OBJECT
public:
    explicit DrmLeaseConnectorV1Interface(DrmLeaseDeviceV1Interface *leaseDevice, KWin::DrmOutput *output);

    uint32_t id() const;
    void send(wl_resource *resource);
    void withdraw();

    DrmLeaseDeviceV1Interface *device() const;
    KWin::DrmOutput *output() const;
    bool withdrawn() const;

private:
    void wp_drm_lease_connector_v1_destroy(Resource *resource) override;

    QPointer<DrmLeaseDeviceV1Interface> m_device;
    bool m_withdrawn = false;
    KWin::DrmOutput *const m_output;
};

class DrmLeaseRequestV1Interface : public QtWaylandServer::wp_drm_lease_request_v1
{
public:
    DrmLeaseRequestV1Interface(DrmLeaseDeviceV1Interface *device, wl_resource *resource);
    ~DrmLeaseRequestV1Interface();

    QVector<DrmLeaseConnectorV1Interface *> connectors() const;
    void invalidate();

protected:
    void wp_drm_lease_request_v1_request_connector(Resource *resource, struct ::wl_resource *connector) override;
    void wp_drm_lease_request_v1_submit(Resource *resource, uint32_t id) override;
    void wp_drm_lease_request_v1_destroy_resource(Resource *resource) override;

    DrmLeaseDeviceV1Interface *const m_device;
    QVector<DrmLeaseConnectorV1Interface *> m_connectors;
    bool m_invalid = false;
};

class DrmLeaseV1Interface : public QObject, private QtWaylandServer::wp_drm_lease_v1
{
    Q_OBJECT
public:
    DrmLeaseV1Interface(DrmLeaseDeviceV1Interface *device, const QVector<DrmLeaseConnectorV1Interface *> &connectors, wl_resource *resource);
    ~DrmLeaseV1Interface();

    void grant(std::unique_ptr<KWin::DrmLease> &&lease);
    void deny();
    void revoke();

    QVector<DrmLeaseConnectorV1Interface *> connectors() const;

private:
    DrmLeaseDeviceV1Interface *m_device;
    QVector<DrmLeaseConnectorV1Interface *> m_connectors;
    std::unique_ptr<KWin::DrmLease> m_lease;
    bool m_finished = false;

    void wp_drm_lease_v1_destroy(Resource *resource) override;
    void wp_drm_lease_v1_destroy_resource(Resource *resource) override;
};

}
