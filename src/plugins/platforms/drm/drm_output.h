/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_OUTPUT_H
#define KWIN_DRM_OUTPUT_H

#include "abstract_wayland_output.h"
#include "drm_pointer.h"
#include "drm_object.h"
#include "drm_object_plane.h"

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <QSharedPointer>
#include <xf86drmMode.h>

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;
class DrmPlane;
class DrmConnector;
class DrmCrtc;
class Cursor;
class DrmGpu;
class DrmPipeline;

class KWIN_EXPORT DrmOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
    ///deletes the output, calling this whilst a page flip is pending will result in an error
    ~DrmOutput() override;

    RenderLoop *renderLoop() const override;

    bool showCursor();
    bool hideCursor();
    bool updateCursor();
    bool moveCursor();
    bool present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion);
    void pageFlipped();
    bool isDpmsEnabled() const;

    DrmPipeline *pipeline() const;
    DrmBuffer *currentBuffer() const;

    bool initCursor(const QSize &cursorSize);

    /**
     * Drm planes might be capable of realizing the current output transform without usage
     * of compositing. This is a getter to query the current state of that
     *
     * @return true if the hardware realizes the transform without further assistance
     */
    bool hardwareTransforms() const;

    DrmGpu *gpu() {
        return m_gpu;
    }

    QSize sourceSize() const;

private:
    friend class DrmGpu;
    friend class DrmBackend;
    DrmOutput(DrmBackend *backend, DrmGpu* gpu, DrmPipeline *pipeline);

    void initOutputDevice();
    bool isCurrentMode(const drmModeModeInfo *mode) const;

    void updateEnablement(bool enable) override;
    void setDrmDpmsMode(DpmsMode mode);
    void setDpmsMode(DpmsMode mode) override;
    void updateMode(int modeIndex) override;
    void updateMode(uint32_t width, uint32_t height, uint32_t refreshRate);
    void updateTransform(Transform transform) override;

    int gammaRampSize() const override;
    bool setGammaRamp(const GammaRamp &gamma) override;
    void setOverscan(uint32_t overscan) override;

    DrmBackend *m_backend;
    DrmGpu *m_gpu;
    DrmPipeline *m_pipeline;
    RenderLoop *m_renderLoop;

    QSharedPointer<DrmDumbBuffer> m_cursor;
    bool m_pageFlipPending = false;
    QTimer m_turnOffTimer;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif
