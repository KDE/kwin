/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drm_abstract_output.h"
#include "drm_object.h"
#include "drm_plane.h"

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QTimer>
#include <chrono>
#include <xf86drmMode.h>

namespace KWin
{

class DrmConnector;
class DrmGpu;
class DrmPipeline;
class DumbSwapchain;
class DrmLease;
class OutputChangeSet;

class KWIN_EXPORT DrmOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    DrmOutput(const std::shared_ptr<DrmConnector> &connector);
    ~DrmOutput() override;

    DrmConnector *connector() const;
    DrmPipeline *pipeline() const;

    bool present(const std::shared_ptr<OutputFrame> &frame) override;
    DrmOutputLayer *primaryLayer() const override;
    DrmOutputLayer *cursorLayer() const override;

    bool queueChanges(const std::shared_ptr<OutputChangeSet> &properties);
    void applyQueuedChanges(const std::shared_ptr<OutputChangeSet> &properties);
    void revertQueuedChanges();
    void updateModes();
    void updateDpmsMode(DpmsMode dpmsMode);

    bool updateCursorLayer() override;

    DrmLease *lease() const;
    bool addLeaseObjects(QList<uint32_t> &objectList);
    void leased(DrmLease *lease);
    void leaseEnded();

    bool setChannelFactors(const QVector3D &rgb) override;
    QVector3D channelFactors() const;
    bool needsColormanagement() const;

private:
    bool setDrmDpmsMode(DpmsMode mode);
    void setDpmsMode(DpmsMode mode) override;
    bool doSetChannelFactors(const QVector3D &rgb);
    ColorDescription createColorDescription(const std::shared_ptr<OutputChangeSet> &props) const;

    QList<std::shared_ptr<OutputMode>> getModes() const;

    DrmPipeline *m_pipeline;
    const std::shared_ptr<DrmConnector> m_connector;

    QTimer m_turnOffTimer;
    DrmLease *m_lease = nullptr;

    QVector3D m_channelFactors = {1, 1, 1};
    bool m_channelFactorsNeedShaderFallback = false;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput *)
