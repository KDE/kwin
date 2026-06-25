/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QPoint>
#include <QSize>

#include <chrono>
#include <xf86drmMode.h>

#include "core/backendoutput.h"
#include "core/colorpipeline.h"
#include "core/colorspace.h"
#include "core/renderloop_p.h"
#include "drm_blob.h"
#include "drm_connector.h"
#include "drm_plane.h"

namespace KWin
{

class DrmGpu;
class DrmConnector;
class DrmCrtc;
class DrmConnectorMode;
class DrmPipelineLayer;
class DrmCommitThread;
class OutputFrame;

class DrmPipeline
{
public:
    DrmPipeline(DrmConnector *conn);
    ~DrmPipeline();

    /**
     * tests the pending commit first and commits it if the test passes
     * if the test fails, there is a guarantee for no lasting changes
     */
    std::expected<void, OutputError> present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame);
    std::expected<void, OutputError> testPresent(const std::shared_ptr<OutputFrame> &frame);
    void maybeModeset(const std::shared_ptr<OutputFrame> &frame);
    void forceLegacyModeset();

    bool needsModeset() const;
    void applyPendingChanges();
    void revertPendingChanges();

    std::expected<void, OutputError> presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay);

    DrmConnector *connector() const;
    DrmGpu *gpu() const;

    void pageFlipped(std::chrono::nanoseconds timestamp);
    bool modesetPresentPending() const;
    void resetModesetPresentPending();
    DrmCommitThread *commitThread() const;

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    QList<DrmPipelineLayer *> layers() const;
    void setLayers(const QList<DrmPipelineLayer *> &layers);
    std::chrono::nanoseconds presentationDeadline() const;

    DrmCrtc *crtc() const;
    std::shared_ptr<DrmConnectorMode> mode() const;
    bool active() const;
    bool activePending() const;
    bool enabled() const;
    PresentationMode presentationMode() const;
    uint32_t overscan() const;
    BackendOutput::RgbRange rgbRange() const;
    DrmConnector::DrmContentType contentType() const;
    const std::shared_ptr<IccProfile> &iccProfile() const;

    void setCrtc(DrmCrtc *crtc);
    void setMode(const std::shared_ptr<DrmConnectorMode> &mode);
    void setActive(bool active);
    void setEnable(bool enable);
    void setPresentationMode(PresentationMode mode);
    void setOverscan(uint32_t overscan);
    void setRgbRange(BackendOutput::RgbRange range);
    void setCrtcColorPipeline(const ColorPipeline &pipeline);
    void setContentType(DrmConnector::DrmContentType type);
    void setIccProfile(const std::shared_ptr<IccProfile> &profile);
    void setHighDynamicRange(bool hdr);
    void setWideColorGamut(bool wcg);
    void setMaxBpc(uint32_t max);

    enum class CommitMode {
        Test,
        TestAllowModeset,
        CommitModeset,
    };
    Q_ENUM(CommitMode)
    static std::expected<void, OutputError> commitPipelines(const QList<DrmPipeline *> &pipelines, DrmGpu *gpu, CommitMode mode, const QList<DrmObject *> &unusedObjects = {});

private:
    bool isBufferForDirectScanout() const;
    uint32_t calculateUnderscan();
    std::shared_ptr<DrmBlob> createHdrMetadata(TransferFunction::Type transferFunction) const;

    // legacy only
    std::expected<void, OutputError> presentLegacy(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame);
    std::expected<void, OutputError> legacyModeset();
    std::expected<void, OutputError> setLegacyGamma();
    std::expected<void, OutputError> applyPendingChangesLegacy();
    std::expected<void, OutputError> setCursorLegacy(DrmPipelineLayer *layer);
    static std::expected<void, OutputError> commitPipelinesLegacy(const QList<DrmPipeline *> &pipelines, DrmGpu *gpu, CommitMode mode, const QList<DrmObject *> &unusedObjects);

    // atomic modesetting only
    std::expected<void, OutputError> prepareAtomicCommit(DrmAtomicCommit *commit, CommitMode mode, const std::shared_ptr<OutputFrame> &frame);
    std::expected<void, OutputError> prepareAtomicModeset(DrmAtomicCommit *commit);
    std::expected<void, OutputError> prepareAtomicPresentation(DrmAtomicCommit *commit, const std::shared_ptr<OutputFrame> &frame);
    std::expected<void, OutputError> prepareAtomicPlane(DrmAtomicCommit *commit, DrmPlane *plane, DrmPipelineLayer *layer, const std::shared_ptr<OutputFrame> &frame);
    void prepareAtomicDisable(DrmAtomicCommit *commit);
    static std::expected<void, OutputError> commitPipelinesAtomic(const QList<DrmPipeline *> &pipelines, DrmGpu *gpu, CommitMode mode, const std::shared_ptr<OutputFrame> &frame, const QList<DrmObject *> &unusedObjects);

    DrmOutput *m_output = nullptr;
    DrmConnector *m_connector = nullptr;

    bool m_modesetPresentPending = false;
    ColorPipeline m_currentLegacyGamma;

    struct State
    {
        DrmCrtc *crtc = nullptr;
        bool active = true; // whether or not the pipeline should be currently used
        bool enabled = true; // whether or not the pipeline needs a crtc
        bool needsModeset = false;
        bool needsModesetProperties = false;
        std::shared_ptr<DrmConnectorMode> mode;
        uint32_t overscan = 0;
        BackendOutput::RgbRange rgbRange = BackendOutput::RgbRange::Automatic;
        PresentationMode presentationMode = PresentationMode::VSync;
        ColorPipeline crtcColorPipeline;
        DrmConnector::DrmContentType contentType = DrmConnector::DrmContentType::Graphics;

        std::shared_ptr<IccProfile> iccProfile;
        bool hdr = false;
        bool wcg = false;
        uint32_t maxBpc = 10;

        QList<DrmPipelineLayer *> layers;
    };
    // the state that is to be tested next
    State m_pending;
    // the state that will be applied at the next real atomic commit
    State m_next;

    std::unique_ptr<DrmCommitThread> m_commitThread;
};

}
