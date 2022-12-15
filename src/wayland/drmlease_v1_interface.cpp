/*
    SPDX-FileCopyrightText: 2021-2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "drmlease_v1_interface.h"
#include "display.h"
#include "drmlease_v1_interface_p.h"
#include "utils.h"
#include "utils/common.h"

#include <fcntl.h>
#include <unistd.h>

namespace KWaylandServer
{

static const quint32 s_version = 1;

DrmLeaseManagerV1::DrmLeaseManagerV1(KWin::DrmBackend *backend, Display *display, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_display(display)
{
    const auto &gpus = m_backend->gpus();
    for (const auto &gpu : gpus) {
        addGpu(gpu.get());
    }
    connect(m_backend, &KWin::DrmBackend::gpuAdded, this, &DrmLeaseManagerV1::addGpu);
    connect(m_backend, &KWin::DrmBackend::gpuRemoved, this, &DrmLeaseManagerV1::removeGpu);
    connect(m_backend, &KWin::DrmBackend::outputsQueried, this, &DrmLeaseManagerV1::handleOutputsQueried);
    connect(m_backend, &KWin::DrmBackend::activeChanged, this, [this]() {
        if (!m_backend->isActive()) {
            for (const auto device : m_leaseDevices) {
                device->setDrmMaster(false);
            }
        }
    });
}

DrmLeaseManagerV1::~DrmLeaseManagerV1()
{
    for (const auto device : m_leaseDevices) {
        device->remove();
    }
}

void DrmLeaseManagerV1::addGpu(KWin::DrmGpu *gpu)
{
    m_leaseDevices[gpu] = new DrmLeaseDeviceV1Interface(m_display, gpu);
}

void DrmLeaseManagerV1::removeGpu(KWin::DrmGpu *gpu)
{
    if (auto device = m_leaseDevices.take(gpu)) {
        device->remove();
    }
}

void DrmLeaseManagerV1::handleOutputsQueried()
{
    for (const auto device : m_leaseDevices) {
        device->done();
        device->setDrmMaster(m_backend->isActive());
    }
}

DrmLeaseDeviceV1Interface::DrmLeaseDeviceV1Interface(Display *display, KWin::DrmGpu *gpu)
    : QtWaylandServer::wp_drm_lease_device_v1(*display, s_version)
    , m_gpu(gpu)
{
    const auto outputs = gpu->drmOutputs();
    for (const auto output : outputs) {
        addOutput(output);
    }
    connect(gpu, &KWin::DrmGpu::outputAdded, this, &DrmLeaseDeviceV1Interface::addOutput);
    connect(gpu, &KWin::DrmGpu::outputRemoved, this, &DrmLeaseDeviceV1Interface::removeOutput);
}

DrmLeaseDeviceV1Interface::~DrmLeaseDeviceV1Interface()
{
    while (!m_connectors.empty()) {
        removeOutput(m_connectors.begin()->first);
    }
}

void DrmLeaseDeviceV1Interface::addOutput(KWin::DrmAbstractOutput *output)
{
    KWin::DrmOutput *drmOutput = qobject_cast<KWin::DrmOutput *>(output);
    if (!drmOutput || !drmOutput->isNonDesktop()) {
        return;
    }
    m_connectors[drmOutput] = std::make_unique<DrmLeaseConnectorV1Interface>(this, drmOutput);

    if (m_hasDrmMaster) {
        offerConnector(m_connectors[drmOutput].get());
    }
}

void DrmLeaseDeviceV1Interface::removeOutput(KWin::DrmAbstractOutput *output)
{
    const auto it = m_connectors.find(output);
    if (it != m_connectors.end()) {
        DrmLeaseConnectorV1Interface *connector = it->second.get();
        connector->withdraw();
        for (const auto &lease : std::as_const(m_leases)) {
            if (lease->connectors().contains(connector)) {
                lease->connectors().removeOne(connector);
                lease->revoke();
            }
        }
        for (const auto &leaseRequest : std::as_const(m_leaseRequests)) {
            if (leaseRequest->connectors().contains(connector)) {
                leaseRequest->invalidate();
            }
        }
        m_connectors.erase(it);
    }
}

void DrmLeaseDeviceV1Interface::setDrmMaster(bool hasDrmMaster)
{
    if (hasDrmMaster == m_hasDrmMaster) {
        return;
    }
    if (hasDrmMaster) {
        // send pending drm fds
        while (!m_pendingFds.isEmpty()) {
            KWin::FileDescriptor fd = m_gpu->createNonMasterFd();
            send_drm_fd(m_pendingFds.dequeue(), fd.get());
        }
        // offer all connectors again
        for (const auto &[output, connector] : m_connectors) {
            offerConnector(connector.get());
        }
    } else {
        // withdraw all connectors
        for (const auto &[output, connector] : m_connectors) {
            connector->withdraw();
        }
        // and revoke all leases
        for (const auto &lease : std::as_const(m_leases)) {
            lease->revoke();
        }
    }
    m_hasDrmMaster = hasDrmMaster;
    done();
}

void DrmLeaseDeviceV1Interface::done()
{
    const auto resources = resourceMap();
    for (const auto resource : resources) {
        send_done(resource->handle);
    }
}

void DrmLeaseDeviceV1Interface::remove()
{
    for (const auto &lease : std::as_const(m_leases)) {
        lease->deny();
    }
    for (const auto &[output, connector] : m_connectors) {
        connector->withdraw();
    }
    for (const auto &request : std::as_const(m_leaseRequests)) {
        request->invalidate();
    }
    done();
    globalRemove();
}

void DrmLeaseDeviceV1Interface::addLeaseRequest(DrmLeaseRequestV1Interface *leaseRequest)
{
    m_leaseRequests.push_back(leaseRequest);
}

void DrmLeaseDeviceV1Interface::removeLeaseRequest(DrmLeaseRequestV1Interface *leaseRequest)
{
    m_leaseRequests.removeOne(leaseRequest);
}

void DrmLeaseDeviceV1Interface::addLease(DrmLeaseV1Interface *lease)
{
    m_leases.push_back(lease);
}

void DrmLeaseDeviceV1Interface::removeLease(DrmLeaseV1Interface *lease)
{
    m_leases.removeOne(lease);
}

bool DrmLeaseDeviceV1Interface::hasDrmMaster() const
{
    return m_hasDrmMaster;
}

KWin::DrmGpu *DrmLeaseDeviceV1Interface::gpu() const
{
    return m_gpu;
}

void DrmLeaseDeviceV1Interface::offerConnector(DrmLeaseConnectorV1Interface *connector)
{
    for (const auto &resource : resourceMap()) {
        auto connectorResource = connector->add(resource->client(), 0, resource->version());
        send_connector(resource->handle, connectorResource->handle);
        connector->send(connectorResource->handle);
    }
}

void DrmLeaseDeviceV1Interface::wp_drm_lease_device_v1_create_lease_request(Resource *resource, uint32_t id)
{
    wl_resource *requestResource = wl_resource_create(resource->client(), &wp_drm_lease_request_v1_interface,
                                                      resource->version(), id);
    if (!requestResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    m_leaseRequests << new DrmLeaseRequestV1Interface(this, requestResource);
}

void DrmLeaseDeviceV1Interface::wp_drm_lease_device_v1_release(Resource *resource)
{
    send_released(resource->handle);
    wl_resource_destroy(resource->handle);
}

void DrmLeaseDeviceV1Interface::wp_drm_lease_device_v1_bind_resource(Resource *resource)
{
    if (isGlobalRemoved()) {
        return;
    }
    if (!m_hasDrmMaster) {
        m_pendingFds << resource->handle;
        return;
    }
    KWin::FileDescriptor fd = m_gpu->createNonMasterFd();
    send_drm_fd(resource->handle, fd.get());
    for (const auto &[output, connector] : m_connectors) {
        if (!connector->withdrawn()) {
            auto connectorResource = connector->add(resource->client(), 0, s_version);
            send_connector(resource->handle, connectorResource->handle);
            connector->send(connectorResource->handle);
        }
    }
    send_done(resource->handle);
}

void DrmLeaseDeviceV1Interface::wp_drm_lease_device_v1_destroy_global()
{
    delete this;
}

DrmLeaseConnectorV1Interface::DrmLeaseConnectorV1Interface(DrmLeaseDeviceV1Interface *leaseDevice, KWin::DrmOutput *output)
    : wp_drm_lease_connector_v1()
    , m_device(leaseDevice)
    , m_output(output)
{
}

uint32_t DrmLeaseConnectorV1Interface::id() const
{
    return m_output->connector()->id();
}

DrmLeaseDeviceV1Interface *DrmLeaseConnectorV1Interface::device() const
{
    return m_device;
}

KWin::DrmOutput *DrmLeaseConnectorV1Interface::output() const
{
    return m_output;
}

bool DrmLeaseConnectorV1Interface::withdrawn() const
{
    return m_withdrawn;
}

void DrmLeaseConnectorV1Interface::send(wl_resource *resource)
{
    m_withdrawn = false;
    send_connector_id(resource, m_output->connector()->id());
    send_name(resource, m_output->name());
    send_description(resource, m_output->description());
    send_done(resource);
}

void DrmLeaseConnectorV1Interface::withdraw()
{
    if (!m_withdrawn) {
        m_withdrawn = true;
        for (const auto &resource : resourceMap()) {
            send_withdrawn(resource->handle);
        }
    }
}

void DrmLeaseConnectorV1Interface::wp_drm_lease_connector_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DrmLeaseRequestV1Interface::DrmLeaseRequestV1Interface(DrmLeaseDeviceV1Interface *device, wl_resource *resource)
    : wp_drm_lease_request_v1(resource)
    , m_device(device)
{
}

DrmLeaseRequestV1Interface::~DrmLeaseRequestV1Interface()
{
    m_device->removeLeaseRequest(this);
}

QVector<DrmLeaseConnectorV1Interface *> DrmLeaseRequestV1Interface::connectors() const
{
    return m_connectors;
}

void DrmLeaseRequestV1Interface::invalidate()
{
    m_connectors.clear();
    m_invalid = true;
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_request_connector(Resource *resource, struct ::wl_resource *connector_handle)
{
    if (auto connector = resource_cast<DrmLeaseConnectorV1Interface *>(connector_handle)) {
        if (connector->device() != m_device) {
            wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_WRONG_DEVICE, "Requested connector from invalid lease device");
        } else if (connector->withdrawn()) {
            qCWarning(KWIN_CORE) << "DrmLease: withdrawn connector requested";
            invalidate();
        } else if (m_connectors.contains(connector)) {
            wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_DUPLICATE_CONNECTOR, "Requested connector twice");
        } else if (!m_invalid) {
            m_connectors << connector;
        }
    } else {
        qCWarning(KWIN_CORE, "DrmLease: Invalid connector requested");
        invalidate();
    }
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_submit(Resource *resource, uint32_t id)
{
    wl_resource *leaseResource = wl_resource_create(resource->client(), &wp_drm_lease_v1_interface, s_version, id);
    if (!leaseResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DrmLeaseV1Interface *lease = new DrmLeaseV1Interface(m_device, m_connectors, leaseResource);
    m_device->addLease(lease);
    if (!m_device->hasDrmMaster()) {
        qCWarning(KWIN_CORE) << "DrmLease: rejecting lease request without drm master";
        lease->deny();
    } else if (m_invalid) {
        qCWarning(KWIN_CORE) << "DrmLease: rejecting lease request with a withdrawn connector";
        lease->deny();
    } else if (m_connectors.isEmpty()) {
        wl_resource_post_error(resource->handle, WP_DRM_LEASE_REQUEST_V1_ERROR_EMPTY_LEASE, "Requested lease without connectors");
    } else {
        QVector<KWin::DrmOutput *> outputs;
        for (const auto &connector : m_connectors) {
            outputs.push_back(connector->output());
        }
        auto drmLease = m_device->gpu()->leaseOutputs(outputs);
        if (drmLease) {
            lease->grant(std::move(drmLease));
        } else {
            lease->deny();
        }
    }
    wl_resource_destroy(resource->handle);
}

void DrmLeaseRequestV1Interface::wp_drm_lease_request_v1_destroy_resource(Resource *resource)
{
    delete this;
}

DrmLeaseV1Interface::DrmLeaseV1Interface(DrmLeaseDeviceV1Interface *device, const QVector<DrmLeaseConnectorV1Interface *> &connectors, wl_resource *resource)
    : wp_drm_lease_v1(resource)
    , m_device(device)
    , m_connectors(connectors)
{
}

DrmLeaseV1Interface::~DrmLeaseV1Interface()
{
    if (m_lease) {
        revoke();
    } else {
        deny();
    }
    m_device->removeLease(this);
}

void DrmLeaseV1Interface::grant(std::unique_ptr<KWin::DrmLease> &&lease)
{
    KWin::FileDescriptor tmp = std::move(lease->fd());
    send_lease_fd(tmp.get());
    m_lease = std::move(lease);
    connect(m_lease.get(), &KWin::DrmLease::revokeRequested, this, &DrmLeaseV1Interface::revoke);
    for (const auto &connector : std::as_const(m_connectors)) {
        connector->withdraw();
    }
    m_device->done();
}

void DrmLeaseV1Interface::deny()
{
    Q_ASSERT(!m_lease);
    if (!m_finished) {
        m_finished = true;
        send_finished();
    }
}

void DrmLeaseV1Interface::revoke()
{
    Q_ASSERT(m_lease);
    if (!m_finished) {
        m_finished = true;
        send_finished();
    }
    m_lease.reset();
    // check if we should offer connectors again
    if (m_device->hasDrmMaster()) {
        for (const auto &connector : std::as_const(m_connectors)) {
            m_device->offerConnector(connector);
        }
        m_device->done();
    }
}

void DrmLeaseV1Interface::wp_drm_lease_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DrmLeaseV1Interface::wp_drm_lease_v1_destroy_resource(Resource *resource)
{
    delete this;
}

QVector<DrmLeaseConnectorV1Interface *> DrmLeaseV1Interface::connectors() const
{
    return m_connectors;
}

}
