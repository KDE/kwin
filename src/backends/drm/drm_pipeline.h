/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QVector>

#include <chrono>
#include <xf86drmMode.h>

#include "colorlut.h"
#include "drm_object_plane.h"
#include "output.h"
#include "renderloop_p.h"

namespace KWin
{

class DrmGpu;
class DrmConnector;
class DrmCrtc;
class GammaRamp;
class DrmConnectorMode;
class DrmPipelineLayer;
class DrmOverlayLayer;

class DrmGammaRamp
{
public:
    DrmGammaRamp(DrmCrtc *crtc, const QSharedPointer<ColorTransformation> &transformation);
    ~DrmGammaRamp();

    const ColorLUT &lut() const;
    uint32_t blobId() const;

private:
    DrmGpu *m_gpu;
    const ColorLUT m_lut;
    uint32_t m_blobId = 0;
};

class DrmPipeline
{
public:
    DrmPipeline(DrmConnector *conn);
    ~DrmPipeline();

    /**
     * tests the pending commit first and commits it if the test passes
     * if the test fails, there is a guarantee for no lasting changes
     */
    bool present();
    bool testScanout();
    bool maybeModeset();

    bool needsModeset() const;
    void applyPendingChanges();
    void revertPendingChanges();

    bool setCursor(const QPoint &hotspot = QPoint());
    bool moveCursor();

    DrmConnector *connector() const;
    DrmCrtc *currentCrtc() const;
    DrmGpu *gpu() const;

    void pageFlipped(std::chrono::nanoseconds timestamp);
    bool pageflipPending() const;
    bool modesetPresentPending() const;
    void resetModesetPresentPending();
    void printDebugInfo() const;
    /**
     * what size buffers submitted to this pipeline should have
     */
    QSize bufferSize() const;

    QMap<uint32_t, QVector<uint64_t>> formats() const;
    QMap<uint32_t, QVector<uint64_t>> cursorFormats() const;
    bool pruneModifier();

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    DrmCrtc *crtc() const;
    QSharedPointer<DrmConnectorMode> mode() const;
    bool active() const;
    bool enabled() const;
    DrmPipelineLayer *primaryLayer() const;
    DrmOverlayLayer *cursorLayer() const;
    DrmPlane::Transformations renderOrientation() const;
    DrmPlane::Transformations bufferOrientation() const;
    RenderLoopPrivate::SyncMode syncMode() const;
    uint32_t overscan() const;
    Output::RgbRange rgbRange() const;

    void setCrtc(DrmCrtc *crtc);
    void setMode(const QSharedPointer<DrmConnectorMode> &mode);
    void setActive(bool active);
    void setEnable(bool enable);
    void setLayers(const QSharedPointer<DrmPipelineLayer> &primaryLayer, const QSharedPointer<DrmOverlayLayer> &cursorLayer);
    void setRenderOrientation(DrmPlane::Transformations orientation);
    void setBufferOrientation(DrmPlane::Transformations orientation);
    void setSyncMode(RenderLoopPrivate::SyncMode mode);
    void setOverscan(uint32_t overscan);
    void setRgbRange(Output::RgbRange range);
    void setColorTransformation(const QSharedPointer<ColorTransformation> &transformation);

    enum class CommitMode {
        Test,
        Commit,
        CommitModeset
    };
    Q_ENUM(CommitMode);
    static bool commitPipelines(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects = {});

private:
    bool activePending() const;
    bool isBufferForDirectScanout() const;
    uint32_t calculateUnderscan();

    // legacy only
    bool presentLegacy();
    bool legacyModeset();
    bool applyPendingChangesLegacy();
    bool setCursorLegacy();
    bool moveCursorLegacy();
    static bool commitPipelinesLegacy(const QVector<DrmPipeline *> &pipelines, CommitMode mode);

    // atomic modesetting only
    bool populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags);
    void atomicCommitFailed();
    void atomicCommitSuccessful(CommitMode mode);
    void prepareAtomicModeset();
    static bool commitPipelinesAtomic(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects);

    // logging helpers
    enum class PrintMode {
        OnlyChanged,
        All,
    };
    static void printFlags(uint32_t flags);
    static void printProps(DrmObject *object, PrintMode mode);

    DrmOutput *m_output = nullptr;
    DrmConnector *m_connector = nullptr;

    bool m_pageflipPending = false;
    bool m_modesetPresentPending = false;

    struct State
    {
        DrmCrtc *crtc = nullptr;
        QMap<uint32_t, QVector<uint64_t>> formats;
        bool active = true; // whether or not the pipeline should be currently used
        bool enabled = true; // whether or not the pipeline needs a crtc
        bool needsModeset = false;
        QSharedPointer<DrmConnectorMode> mode;
        uint32_t overscan = 0;
        Output::RgbRange rgbRange = Output::RgbRange::Automatic;
        RenderLoopPrivate::SyncMode syncMode = RenderLoopPrivate::SyncMode::Fixed;
        QSharedPointer<ColorTransformation> colorTransformation;
        QSharedPointer<DrmGammaRamp> gamma;

        QSharedPointer<DrmPipelineLayer> layer;
        QSharedPointer<DrmOverlayLayer> cursorLayer;
        QPoint cursorHotspot;

        // the transformation that this pipeline will apply to submitted buffers
        DrmPlane::Transformations bufferOrientation = DrmPlane::Transformation::Rotate0;
        // the transformation that buffers submitted to the pipeline should have
        DrmPlane::Transformations renderOrientation = DrmPlane::Transformation::Rotate0;
    };
    // the state that is to be tested next
    State m_pending;
    // the state that will be applied at the next real atomic commit
    State m_next;
    // the state that is already committed
    State m_current;
};

}
