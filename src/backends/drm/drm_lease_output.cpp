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
    , DrmDisplayDevice(pipeline->gpu())
    , m_pipeline(pipeline)
{
    m_pipeline->setDisplayDevice(this);
    qCDebug(KWIN_DRM) << "offering connector" << m_pipeline->connector()->id() << "for lease";
}

DrmLeaseOutput::~DrmLeaseOutput()
{
    m_pipeline->setDisplayDevice(nullptr);
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

bool DrmLeaseOutput::present()
{
    return false;
}

bool DrmLeaseOutput::testScanout()
{
    return false;
}

DrmPlane::Transformations DrmLeaseOutput::softwareTransforms() const
{
    return DrmPlane::Transformation::Rotate0;
}

QSize DrmLeaseOutput::bufferSize() const
{
    return m_pipeline->bufferSize();
}

QSize DrmLeaseOutput::sourceSize() const
{
    return m_pipeline->sourceSize();
}

bool DrmLeaseOutput::isFormatSupported(uint32_t drmFormat) const
{
    return m_pipeline->isFormatSupported(drmFormat);
}

QVector<uint64_t> DrmLeaseOutput::supportedModifiers(uint32_t drmFormat) const
{
    return m_pipeline->supportedModifiers(drmFormat);
}

int DrmLeaseOutput::maxBpc() const
{
    if (const auto prop = m_pipeline->connector()->getProp(DrmConnector::PropertyIndex::MaxBpc)) {
        return prop->maxValue();
    } else {
        return 8;
    }
}

QRect DrmLeaseOutput::renderGeometry() const
{
    return QRect(QPoint(), m_pipeline->sourceSize());
}

DrmLayer *DrmLeaseOutput::outputLayer() const
{
    return m_pipeline->pending.layer.data();
}

void DrmLeaseOutput::frameFailed() const
{
}

void DrmLeaseOutput::pageFlipped(std::chrono::nanoseconds timestamp) const
{
    Q_UNUSED(timestamp)
}

}
