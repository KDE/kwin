/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QVector>
#include <KWaylandServer/drmleasedevice_v1_interface.h>

namespace KWin
{

class DrmConnector;
class DrmPipeline;

/**
 * a DrmLeaseOutput represents a non-desktop output (usually a VR headset)
 * that is not used directly by the compositor but is instead leased out to
 * applications (usually VR compositors) that drive the output themselves
 */
class DrmLeaseOutput : public KWaylandServer::DrmLeaseConnectorV1Interface
{
    Q_OBJECT
public:
    DrmLeaseOutput(DrmPipeline *pipeline, KWaylandServer::DrmLeaseDeviceV1Interface *leaseDevice);
    ~DrmLeaseOutput() override;

    bool addLeaseObjects(QVector<uint32_t> &objectList);
    void leased(KWaylandServer::DrmLeaseV1Interface *lease);
    void leaseEnded();

    KWaylandServer::DrmLeaseV1Interface *lease() const;
    DrmPipeline *pipeline() const;

private:
    DrmPipeline *m_pipeline;
    KWaylandServer::DrmLeaseV1Interface *m_lease = nullptr;
};

}
