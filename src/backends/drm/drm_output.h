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
#include <chrono>

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
class DumbSwapchain;

class KWIN_EXPORT DrmOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    DrmOutput(DrmPipeline *pipeline);
    ~DrmOutput() override;

    bool present(const QSharedPointer<DrmBuffer> &buffer, QRegion damagedRegion) override;

    DrmConnector *connector() const;
    DrmPipeline *pipeline() const;

    QSize bufferSize() const override;
    QSize sourceSize() const override;
    bool isFormatSupported(uint32_t drmFormat) const override;
    QVector<uint64_t> supportedModifiers(uint32_t drmFormat) const override;
    bool needsSoftwareTransformation() const override;
    int maxBpc() const override;

    bool queueChanges(const WaylandOutputConfig &config);
    void applyQueuedChanges(const WaylandOutputConfig &config);
    void revertQueuedChanges();
    void updateModes();

    void pageFlipped(std::chrono::nanoseconds timestamp);
    void presentFailed();
    bool usesSoftwareCursor() const override;

private:
    void initOutputDevice();

    void updateEnablement(bool enable) override;
    bool setDrmDpmsMode(DpmsMode mode);
    void setDpmsMode(DpmsMode mode) override;

    QVector<AbstractWaylandOutput::Mode> getModes() const;

    int gammaRampSize() const override;
    bool setGammaRamp(const GammaRamp &gamma) override;
    void updateCursor();
    void moveCursor();

    DrmPipeline *m_pipeline;
    DrmConnector *m_connector;

    QSharedPointer<DumbSwapchain> m_cursor;
    bool m_setCursorSuccessful = false;
    bool m_moveCursorSuccessful = false;
    QRect m_lastCursorGeometry;
    QTimer m_turnOffTimer;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif
