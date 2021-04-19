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

namespace KWin {

class DrmGpu;
class DrmConnector;
class DrmCrtc;
class DrmBuffer;
class DrmDumbBuffer;
class GammaRamp;

class DrmPipeline
{
public:
    DrmPipeline(void *pageflipUserData, DrmGpu *gpu, DrmConnector *conn, DrmCrtc *crtc, DrmPlane *primaryPlane, DrmPlane *cursorPlane);
    ~DrmPipeline();

    /**
     * tests the pending commit first and commits it if the test passes
     * if the test fails, there is a guarantee for no lasting changes
     */
    bool present(const QSharedPointer<DrmBuffer> &buffer);

    /**
     * tests the pending commit
     * always returns true in legacy mode!
     */
    bool test();

    bool modeset(QSize source, drmModeModeInfo mode);
    bool setCursor(const QSharedPointer<DrmDumbBuffer> &buffer);
    bool setEnablement(bool enable);
    bool setGammaRamp(const GammaRamp &ramp);
    bool setTransformation(const QSize &srcSize, const DrmPlane::Transformations &transformation);

    void setPrimaryBuffer(const QSharedPointer<DrmBuffer> &buffer);
    bool moveCursor(QPoint pos);

    bool addOverlayPlane(DrmPlane *plane);

    DrmPlane::Transformations transformation() const;

private:
    bool atomicCommit(bool testOnly);
    bool populateAtomicValues(drmModeAtomicReq *req, uint32_t &flags);
    bool presentLegacy();
    void checkTestBuffer();

    void *m_pageflipUserData = nullptr;
    DrmGpu *m_gpu = nullptr;
    DrmConnector *m_connector = nullptr;
    DrmCrtc *m_crtc = nullptr;

    DrmPlane *m_primaryPlane = nullptr;
    QSharedPointer<DrmBuffer> m_primaryBuffer;
    QSharedPointer<DrmBuffer> m_testBuffer;

    QVector<DrmPlane*> m_overlayPlanes;
    QVector<QSharedPointer<DrmBuffer>> m_overlayBuffers;

    struct {
        bool changed = false;
        bool enabled = true;
        QSize sourceSize = QSize(-1, -1);
        drmModeModeInfo mode;
        uint32_t blobId = 0;
        DrmPlane::Transformations transformation = DrmPlane::Transformation::Rotate0;
    } m_mode;
    struct {
        DrmPlane *plane = nullptr;
        QPoint pos = QPoint(100, 100);
        QSharedPointer<DrmDumbBuffer> buffer;
    } m_cursor;
    struct {
        bool changed = false;
        uint32_t blobId = 0;
    } m_gamma;

};

}
