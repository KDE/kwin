/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPoint>
#include <QSize>
#include <QVector>
#include <QSharedPointer>

#include <xf86drmMode.h>
#include <chrono>

#include "drm_object_plane.h"
#include "renderloop_p.h"
#include "abstract_wayland_output.h"

namespace KWin
{

class DrmGpu;
class DrmConnector;
class DrmCrtc;
class DrmBuffer;
class DrmDumbBuffer;
class GammaRamp;

class DrmGammaRamp
{
public:
    DrmGammaRamp(DrmGpu *gpu, const GammaRamp &lut);
    ~DrmGammaRamp();

    uint32_t size() const;
    uint16_t *red() const;
    uint16_t *green() const;
    uint16_t *blue() const;
    uint32_t blobId() const;

private:
    DrmGpu *m_gpu;
    const GammaRamp m_lut;
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
    bool present(const QSharedPointer<DrmBuffer> &buffer);

    bool needsModeset() const;
    bool needsCommit() const;
    void prepareModeset();
    void applyPendingChanges();
    void revertPendingChanges();

    bool setCursor(const QSharedPointer<DrmDumbBuffer> &buffer, const QPoint &hotspot = QPoint());
    bool moveCursor(QPoint pos);

    bool isCursorVisible() const;
    QPoint cursorPos() const;

    DrmConnector *connector() const;
    DrmCrtc *currentCrtc() const;
    DrmGpu *gpu() const;

    void pageFlipped(std::chrono::nanoseconds timestamp);
    bool pageflipPending() const;
    bool modesetPresentPending() const;
    void resetModesetPresentPending();
    void printDebugInfo() const;
    QSize sourceSize() const;

    bool isFormatSupported(uint32_t drmFormat) const;
    QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const;

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    struct State {
        DrmCrtc *crtc = nullptr;
        bool active = true; // whether or not the pipeline should be currently used
        bool enabled = true;// whether or not the pipeline needs a crtc
        int modeIndex = 0;
        uint32_t overscan = 0;
        AbstractWaylandOutput::RgbRange rgbRange = AbstractWaylandOutput::RgbRange::Automatic;
        RenderLoopPrivate::SyncMode syncMode = RenderLoopPrivate::SyncMode::Fixed;
        QSharedPointer<DrmGammaRamp> gamma;
        DrmPlane::Transformations transformation = DrmPlane::Transformation::Rotate0;
    };
    State pending;

    enum class CommitMode {
        Test,
        Commit,
        CommitModeset
    };
    Q_ENUM(CommitMode);
    static bool commitPipelines(const QVector<DrmPipeline*> &pipelines, CommitMode mode, const QVector<DrmObject*> &unusedObjects = {});

private:
    bool populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags);
    bool presentLegacy();
    bool checkTestBuffer();
    bool activePending() const;

    bool applyPendingChangesLegacy();
    bool legacyModeset();

    static void printFlags(uint32_t flags);
    enum class PrintMode { OnlyChanged, All };
    static void printProps(DrmObject *object, PrintMode mode);

    DrmOutput *m_output = nullptr;
    DrmConnector *m_connector = nullptr;

    QSharedPointer<DrmBuffer> m_primaryBuffer;
    QSharedPointer<DrmBuffer> m_oldTestBuffer;
    bool m_pageflipPending = false;
    bool m_modesetPresentPending = false;

    // the state that will be applied at the next real atomic commit
    State m_next;
    // the state that is already committed
    State m_current;
};

}
