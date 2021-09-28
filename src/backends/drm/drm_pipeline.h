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

    const GammaRamp lut;
    drm_color_lut *atomicLut = nullptr;
    uint32_t size;
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
    void prepareModeset();
    void applyPendingChanges();
    void revertPendingChanges();

    bool setCursor(const QSharedPointer<DrmDumbBuffer> &buffer, const QPoint &hotspot = QPoint());
    bool moveCursor(QPoint pos);

    bool isCursorVisible() const;
    QPoint cursorPos() const;

    DrmConnector *connector() const;
    DrmGpu *gpu() const;

    void pageFlipped();
    void printDebugInfo() const;
    QSize sourceSize() const;
    void updateProperties();

    bool isFormatSupported(uint32_t drmFormat) const;
    QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const;

    void setOutput(DrmOutput *output);
    DrmOutput *output() const;

    struct State {
        DrmCrtc *crtc = nullptr;
        bool active = true;
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
        Commit
    };
    Q_ENUM(CommitMode);
    static bool commitPipelines(const QVector<DrmPipeline*> &pipelines, CommitMode mode);

private:
    bool populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags);
    bool presentLegacy();
    bool checkTestBuffer();
    bool activePending() const;

    bool applyPendingChangesLegacy();
    bool legacyModeset();

    DrmOutput *m_output = nullptr;
    DrmConnector *m_connector = nullptr;

    QSharedPointer<DrmBuffer> m_primaryBuffer;
    QSharedPointer<DrmBuffer> m_oldTestBuffer;

    QMap<uint32_t, QVector<uint64_t>> m_formats;
    int m_lastFlags = 0;

    // the state that will be applied at the next real atomic commit
    State m_next;
    // the state that is already committed
    State m_current;
};

}
