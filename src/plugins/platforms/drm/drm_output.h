/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_OUTPUT_H
#define KWIN_DRM_OUTPUT_H

#include "drm_abstract_output.h"
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

class KWIN_EXPORT DrmOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    ///deletes the output, calling this whilst a page flip is pending will result in an error
    ~DrmOutput() override;

    bool initCursor(const QSize &cursorSize) override;
    bool showCursor() override;
    bool hideCursor() override;
    bool updateCursor() override;
    bool moveCursor() override;

    bool present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion) override;
    void pageFlipped();
    bool isDpmsEnabled() const override;

    DrmPipeline *pipeline() const;
    GbmBuffer *currentBuffer() const override;
    QSize sourceSize() const override;

private:
    friend class DrmGpu;
    friend class DrmBackend;
    DrmOutput(DrmGpu* gpu, DrmPipeline *pipeline);

    void initOutputDevice();
    bool isCurrentMode(const drmModeModeInfo *mode) const;

    void updateEnablement(bool enable) override;
    void setDrmDpmsMode(DpmsMode mode);
    void setDpmsMode(DpmsMode mode) override;
    void updateMode(int modeIndex) override;
    void updateMode(uint32_t width, uint32_t height, uint32_t refreshRate) override;
    void updateTransform(Transform transform) override;

    int gammaRampSize() const override;
    bool setGammaRamp(const GammaRamp &gamma) override;
    void setOverscan(uint32_t overscan) override;

    DrmPipeline *m_pipeline;

    QSharedPointer<DrmDumbBuffer> m_cursor;
    bool m_pageFlipPending = false;
    QTimer m_turnOffTimer;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif
