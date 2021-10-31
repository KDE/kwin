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

void DrmLeaseOutput::addLeaseObjects(QVector<uint32_t> &objectList)
{
    qCDebug(KWIN_DRM) << "adding connector" << m_pipeline->connector()->id() << "to lease";
    objectList << m_pipeline->connector()->id();
    objectList << m_pipeline->crtc()->id();
    if (m_pipeline->primaryPlane()) {
        objectList << m_pipeline->primaryPlane()->id();
    }
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

void DrmLeaseOutput::setPipeline(DrmPipeline *pipeline)
{
    Q_ASSERT_X(pipeline->connector() == m_pipeline->connector(), "DrmLeaseOutput::setPipeline", "Pipeline with wrong connector set!");
    delete m_pipeline;
    m_pipeline = pipeline;
}

}
