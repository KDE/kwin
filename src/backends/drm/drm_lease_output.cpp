/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "drm_lease_output.h"

#include "KWaylandServer/drmleasedevice_v1_interface.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "drm_pipeline.h"

#include "logging.h"

#include <QTimer>

namespace KWin
{

DrmLeaseOutput::DrmLeaseOutput(DrmPipeline *pipeline, KWaylandServer::DrmLeaseDeviceV1Interface *leaseDevice)
    : KWaylandServer::DrmLeaseConnectorV1Interface(
        leaseDevice,
        pipeline->connector()->id(),
        pipeline->connector()->modelName(),
        QStringLiteral("%1 %2").arg(pipeline->connector()->edid()->manufacturerString(), pipeline->connector()->modelName())
    )
    , m_pipeline(pipeline)
{
    qCDebug(KWIN_DRM) << "offering connector" << m_pipeline->connector()->id() << "for lease";
}

DrmLeaseOutput::~DrmLeaseOutput()
{
    qCDebug(KWIN_DRM) << "revoking lease offer for connector" << m_pipeline->connector()->id();
}

bool DrmLeaseOutput::addLeaseObjects(QVector<uint32_t> &objectList)
{
    if (!m_pipeline->pending.crtc) {
        qCWarning(KWIN_DRM) << "Can't lease connector: No suitable crtc available";
        return false;
    }
    qCDebug(KWIN_DRM) << "adding connector" << m_pipeline->connector()->id() << "to lease";
    objectList << m_pipeline->connector()->id();
    objectList << m_pipeline->pending.crtc->id();
    if (m_pipeline->pending.crtc->primaryPlane()) {
        objectList << m_pipeline->pending.crtc->primaryPlane()->id();
    }
    return true;
}

void DrmLeaseOutput::leased(KWaylandServer::DrmLeaseV1Interface *lease)
{
    m_lease = lease;
}

void DrmLeaseOutput::leaseEnded()
{
    qCDebug(KWIN_DRM) << "ended lease for connector" << m_pipeline->connector()->id();
    m_lease = nullptr;
}

KWaylandServer::DrmLeaseV1Interface *DrmLeaseOutput::lease() const
{
    return m_lease;
}

DrmPipeline *DrmLeaseOutput::pipeline() const
{
    return m_pipeline;
}

}
