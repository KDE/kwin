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

#include <QObject>
#include <QPoint>
#include <QSharedPointer>
#include <QSize>
#include <QTimer>
#include <QVector>
#include <chrono>
#include <xf86drmMode.h>

namespace KWin
{

class DrmConnector;
class DrmGpu;
class DrmPipeline;
class DumbSwapchain;
class GLTexture;
class RenderTarget;

class KWIN_EXPORT DrmOutput : public DrmAbstractOutput
{
    Q_OBJECT
public:
    DrmOutput(DrmPipeline *pipeline);
    ~DrmOutput() override;

    DrmConnector *connector() const;
    DrmPipeline *pipeline() const;

    bool present() override;
    DrmOutputLayer *outputLayer() const override;

    bool queueChanges(const OutputConfiguration &config);
    void applyQueuedChanges(const OutputConfiguration &config);
    void revertQueuedChanges();
    void updateModes();

    bool usesSoftwareCursor() const override;
    void updateCursor();
    void moveCursor();

    void setColorTransformation(const QSharedPointer<ColorTransformation> &transformation) override;

private:
    void updateEnablement(bool enable) override;
    bool setDrmDpmsMode(DpmsMode mode);
    void setDpmsMode(DpmsMode mode) override;

    QList<QSharedPointer<OutputMode>> getModes() const;

    void renderCursorOpengl(const RenderTarget &renderTarget, const QSize &cursorSize);
    void renderCursorQPainter(const RenderTarget &renderTarget);

    DrmPipeline *m_pipeline;
    DrmConnector *m_connector;

    bool m_setCursorSuccessful = false;
    bool m_moveCursorSuccessful = false;
    bool m_cursorTextureDirty = true;
    std::unique_ptr<GLTexture> m_cursorTexture;
    QTimer m_turnOffTimer;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput *)

#endif
