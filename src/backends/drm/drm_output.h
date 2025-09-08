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
#include "utils/filedescriptor.h"

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
    explicit DrmOutput(const std::shared_ptr<DrmConnector> &connector, DrmPipeline *pipeline);

    DrmConnector *connector() const;
    DrmPipeline *pipeline() const;

    bool testPresentation(const std::shared_ptr<OutputFrame> &frame) override;
    bool present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame) override;
    void repairPresentation() override;
    bool overlayLayersLikelyBroken() const override;

    bool queueChanges(const std::shared_ptr<OutputChangeSet> &properties);
    void applyQueuedChanges(const std::shared_ptr<OutputChangeSet> &properties);
    void revertQueuedChanges();

    bool shouldDisableNonPrimaryPlanes() const;
    bool presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay) override;
    void setAutoRotateAvailable(bool isAvailable) override;

    DrmLease *lease() const;
    bool addLeaseObjects(QList<uint32_t> &objectList);
    void leased(DrmLease *lease);
    void leaseEnded();

    bool setChannelFactors(const QVector3D &rgb) override;
    void updateConnectorProperties();

    /**
     * @returns whether or not the renderer should apply channel factors
     */
    bool needsShadowBuffer() const;

    void removePipeline();

private:
    void tryKmsColorOffloading(State &next);
    std::shared_ptr<ColorDescription> createColorDescription(const State &next) const;
    Capabilities computeCapabilities() const;
    void updateInformation();
    void unsetBrightnessDevice() override;
    void updateBrightness(double newBrightness, double newArtificialHdrHeadroom);
    void maybeScheduleRepaints(const State &next);
    std::optional<uint32_t> decideAutomaticBpcLimit() const;

    QList<std::shared_ptr<OutputMode>> getModes(const State &state) const;

    DrmGpu *const m_gpu;
    DrmPipeline *m_pipeline;
    const std::shared_ptr<DrmConnector> m_connector;

    DrmLease *m_lease = nullptr;

    QVector3D m_sRgbChannelFactors = {1, 1, 1};
    bool m_needsShadowBuffer = false;
    PresentationMode m_desiredPresentationMode = PresentationMode::VSync;
    bool m_autoRotateAvailable = false;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput *)
