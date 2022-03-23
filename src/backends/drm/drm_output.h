/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_OUTPUT_H
#define KWIN_DRM_OUTPUT_H

#include "drm_abstract_output.h"
#include "drm_object.h"
#include "drm_object_plane.h"
#include "renderoutput.h"

#include <QObject>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QVector>
#include <chrono>
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
class DumbSwapchain;
class DrmRenderOutput;

class DrmOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    DrmOutput(const QVector<DrmConnector *> &connectors);
    ~DrmOutput() override;

    bool present() override;
    void pageFlipped(std::chrono::nanoseconds timestamp) const override;
    QVector<QSharedPointer<RenderOutput>> renderOutputs() const override;

    bool queueChanges(const WaylandOutputConfig &config);
    void applyQueuedChanges(const WaylandOutputConfig &config);
    void revertQueuedChanges();
    void updateModes();

    QVector<DrmConnector *> connectors() const;

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
    void applyPipelineChanges();
    void revertPipelineChanges();

    const QVector<DrmConnector *> m_connectors;
    QVector<DrmPipeline *> m_pipelines;
    QVector<QSharedPointer<RenderOutput>> m_renderOutputs;

    QTimer m_turnOffTimer;
};

class DrmRenderOutput : public RenderOutput
{
public:
    DrmRenderOutput(DrmOutput *output, DrmPipeline *pipeline);

    bool usesSoftwareCursor() const override;
    OutputLayer *layer() const override;
    QRect geometry() const override;

protected:
    void updateCursor();
    void moveCursor();

    QSharedPointer<DumbSwapchain> m_cursor;
    bool m_setCursorSuccessful;
    bool m_moveCursorSuccessful;
    QRect m_lastCursorGeometry;

    DrmPipeline *const m_pipeline;
};
}

Q_DECLARE_METATYPE(KWin::DrmOutput *)

#endif
