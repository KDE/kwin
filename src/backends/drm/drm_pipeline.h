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

#include "core/colorlut.h"
#include "core/colorspace.h"
#include "core/output.h"
#include "core/renderloop_p.h"
#include "drm_blob.h"
#include "drm_connector.h"
#include "drm_plane.h"

namespace KWin
{

class DrmGpu;
class DrmConnector;
class DrmCrtc;
class GammaRamp;
class DrmConnectorMode;
class DrmPipelineLayer;
class DrmCommitThread;

class DrmGammaRamp
{
public:
    DrmGammaRamp(DrmCrtc *crtc, const std::shared_ptr<ColorTransformation> &transformation);

    const ColorLUT &lut() const;
    std::shared_ptr<DrmBlob> blob() const;

private:
    const ColorLUT m_lut;
    std::shared_ptr<DrmBlob> m_blob;
};

class DrmPipeline
{
public:
    DrmPipeline(DrmConnector *conn);
    ~DrmPipeline();

    enum class Error {
        None,
        OutofMemory,
        InvalidArguments,
        NoPermission,
        FramePending,
        TestBufferFailed,
        Unknown,
    };
    Q_ENUM(Error)

    /**
     * tests the pending commit first and commits it if the test passes
     * if the test fails, there is a guarantee for no lasting changes
     */
    Error present();
    bool testScanout();
    bool maybeModeset();
    void forceLegacyModeset();

    bool needsModeset() const;
    void applyPendingChanges();
    void revertPendingChanges();

    bool updateCursor();

    DrmConnector *connector() const;
    DrmGpu *gpu() const;

    enum class PageflipType {
        Normal,
        CursorOnly,
        Modeset,
    };
    void pageFlipped(std::chrono::nanoseconds timestamp, PageflipType type, PresentationMode mode);
    bool pageflipsPending() const;
    bool modesetPresentPending() const;
    void resetModesetPresentPending();

    QMap<uint32_t, QList<uint64_t>> formats() const;
    QMap<uint32_t, QList<uint64_t>> cursorFormats() const;
    bool hasCTM() const;
    bool hasGammaRamp() const;
    bool pruneModifier();

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    void setLayers(const std::shared_ptr<DrmPipelineLayer> &primaryLayer, const std::shared_ptr<DrmPipelineLayer> &cursorLayer);
    DrmPipelineLayer *primaryLayer() const;
    DrmPipelineLayer *cursorLayer() const;

    DrmCrtc *crtc() const;
    std::shared_ptr<DrmConnectorMode> mode() const;
    bool active() const;
    bool activePending() const;
    bool enabled() const;
    DrmPlane::Transformations renderOrientation() const;
    PresentationMode presentationMode() const;
    uint32_t overscan() const;
    Output::RgbRange rgbRange() const;
    DrmConnector::DrmContentType contentType() const;
    const ColorDescription &colorDescription() const;
    const std::shared_ptr<IccProfile> &iccProfile() const;

    void setCrtc(DrmCrtc *crtc);
    void setMode(const std::shared_ptr<DrmConnectorMode> &mode);
    void setActive(bool active);
    void setEnable(bool enable);
    void setRenderOrientation(DrmPlane::Transformations orientation);
    void setPresentationMode(PresentationMode mode);
    void setOverscan(uint32_t overscan);
    void setRgbRange(Output::RgbRange range);
    void setGammaRamp(const std::shared_ptr<ColorTransformation> &transformation);
    void setCTM(const QMatrix3x3 &ctm);
    void setContentType(DrmConnector::DrmContentType type);
    void setBT2020(bool useBT2020);
    void setNamedTransferFunction(NamedTransferFunction tf);
    void setIccProfile(const std::shared_ptr<IccProfile> &profile);
    void setSdrBrightness(double sdrBrightness);
    void setSdrGamutWideness(double sdrGamutWideness);
    void setBrightnessOverrides(std::optional<double> peakBrightnessOverride, std::optional<double> averageBrightnessOverride, std::optional<double> minBrightnessOverride);

    enum class CommitMode {
        Test,
        TestAllowModeset,
        CommitModeset
    };
    Q_ENUM(CommitMode)
    static Error commitPipelines(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects = {});

private:
    bool isBufferForDirectScanout() const;
    uint32_t calculateUnderscan();
    static Error errnoToError();
    std::shared_ptr<DrmBlob> createHdrMetadata(NamedTransferFunction transferFunction) const;
    ColorDescription createColorDescription() const;

    // legacy only
    Error presentLegacy();
    Error legacyModeset();
    Error applyPendingChangesLegacy();
    bool setCursorLegacy();
    static Error commitPipelinesLegacy(const QList<DrmPipeline *> &pipelines, CommitMode mode);

    // atomic modesetting only
    Error prepareAtomicCommit(DrmAtomicCommit *commit, CommitMode mode);
    bool prepareAtomicModeset(DrmAtomicCommit *commit);
    Error prepareAtomicPresentation(DrmAtomicCommit *commit);
    void prepareAtomicCursor(DrmAtomicCommit *commit);
    void prepareAtomicDisable(DrmAtomicCommit *commit);
    static Error commitPipelinesAtomic(const QList<DrmPipeline *> &pipelines, CommitMode mode, const QList<DrmObject *> &unusedObjects);

    DrmOutput *m_output = nullptr;
    DrmConnector *m_connector = nullptr;

    bool m_legacyPageflipPending = false;
    bool m_modesetPresentPending = false;

    struct State
    {
        DrmCrtc *crtc = nullptr;
        QMap<uint32_t, QList<uint64_t>> formats;
        bool active = true; // whether or not the pipeline should be currently used
        bool enabled = true; // whether or not the pipeline needs a crtc
        bool needsModeset = false;
        bool needsModesetProperties = false;
        std::shared_ptr<DrmConnectorMode> mode;
        uint32_t overscan = 0;
        Output::RgbRange rgbRange = Output::RgbRange::Automatic;
        PresentationMode presentationMode = PresentationMode::VSync;
        std::shared_ptr<ColorTransformation> colorTransformation;
        std::shared_ptr<DrmGammaRamp> gamma;
        std::shared_ptr<DrmBlob> ctm;
        DrmConnector::DrmContentType contentType = DrmConnector::DrmContentType::Graphics;

        bool BT2020 = false;
        NamedTransferFunction transferFunction = NamedTransferFunction::sRGB;
        double sdrBrightness = 200;
        double sdrGamutWideness = 0;
        std::shared_ptr<IccProfile> iccProfile;
        ColorDescription colorDescription = ColorDescription::sRGB;
        std::optional<double> peakBrightnessOverride;
        std::optional<double> averageBrightnessOverride;
        std::optional<double> minBrightnessOverride;

        // the transformation that buffers submitted to the pipeline should have
        DrmPlane::Transformations renderOrientation = DrmPlane::Transformation::Rotate0;
    };
    // the state that is to be tested next
    State m_pending;
    // the state that will be applied at the next real atomic commit
    State m_next;

    std::unique_ptr<DrmCommitThread> m_commitThread;
    std::shared_ptr<DrmPipelineLayer> m_primaryLayer;
    std::shared_ptr<DrmPipelineLayer> m_cursorLayer;
};

}
