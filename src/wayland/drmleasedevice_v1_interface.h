/*
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QPointer>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class DrmLeaseDeviceV1InterfacePrivate;
class DrmLeaseV1Interface;
class DrmLeaseV1InterfacePrivate;
class DrmLeaseRequestV1Interface;
class DrmLeaseConnectorV1InterfacePrivate;

/**
 * The DrmLeaseV1DeviceInterface allows the wayland compositor to offer unused
 * drm connectors for lease by clients. The main use for this is VR headsets
 */
class KWIN_EXPORT DrmLeaseDeviceV1Interface : public QObject
{
    Q_OBJECT
public:
    /**
     * @param createNonMasterFd a function that creates non-master drm file descriptors for
     *          this device that clients can use to enumerate connectors and their properties
     */
    explicit DrmLeaseDeviceV1Interface(Display *display, std::function<int()> createNonMasterFd);
    ~DrmLeaseDeviceV1Interface() override;

    /**
     * Must be called by the compositor when it loses or gains drm master
     */
    void setDrmMaster(bool hasDrmMaster);

    /**
     * Must be called after connectors have been added or removed
     */
    void done();

Q_SIGNALS:
    /**
     * Emitted when a lease is requested. The compositor needs to either
     * grant or deny the lease when receiving this signal
     */
    void leaseRequested(DrmLeaseV1Interface *leaseRequest);

    /**
     * Emitted when a granted lease gets revoked
     */
    void leaseRevoked(DrmLeaseV1Interface *lease);

private:
    friend class DrmLeaseDeviceV1InterfacePrivate;
    DrmLeaseDeviceV1InterfacePrivate *d;
};

/**
 * Represents a lease offer from the compositor. Creating the DrmLeaseConnectorV1Interface
 * will allow clients to requests a lease for the connector, deleting it will result in the
 * offer and possibly an active lease being revoked
 */
class KWIN_EXPORT DrmLeaseConnectorV1Interface : public QObject
{
    Q_OBJECT
public:
    explicit DrmLeaseConnectorV1Interface(DrmLeaseDeviceV1Interface *leaseDevice, uint32_t id, const QString &name, const QString &description);
    ~DrmLeaseConnectorV1Interface() override;

    uint32_t id() const;

    static DrmLeaseConnectorV1Interface *get(wl_resource *resource);

private:
    friend class DrmLeaseConnectorV1InterfacePrivate;
    std::unique_ptr<DrmLeaseConnectorV1InterfacePrivate> d;
};

/**
 * Represents a lease request or active lease
 */
class KWIN_EXPORT DrmLeaseV1Interface : public QObject
{
    Q_OBJECT
public:
    /**
     * Grant the client requesting the lease access to DRM resources needed to
     * drive the outputs corresponding to the requested connectors.
     * Must only be called once in response to DrmLeaseDeviceV1Interface::leaseRequested
     */
    void grant(int leaseFd, uint32_t lesseeId);

    /**
     * Deny the lease request. The compositor may call this in response to
     * DrmLeaseDeviceV1Interface::leaseRequested
     */
    void deny();

    /**
     * revoke a granted lease request and offer the leased connectors again
     */
    void revoke();

    /**
     * The connectors this lease (request) encompasses
     */
    QVector<DrmLeaseConnectorV1Interface *> connectors() const;

    /**
     * The lesseeId passed to DrmLeaseV1Interface::grant, or 0 if this lease was not granted
     */
    uint32_t lesseeId() const;

private:
    DrmLeaseV1Interface(DrmLeaseDeviceV1InterfacePrivate *device, wl_resource *resource);
    ~DrmLeaseV1Interface();

    friend class DrmLeaseDeviceV1InterfacePrivate;
    friend class DrmLeaseRequestV1Interface;
    friend class DrmLeaseV1InterfacePrivate;
    std::unique_ptr<DrmLeaseV1InterfacePrivate> d;
};

}
