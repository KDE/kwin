/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "qpainterbackend.h"
#include "drm_render_backend.h"
#include "dumb_swapchain.h"

#include <QObject>
#include <QVector>
#include <QSharedPointer>

namespace KWin
{

class DrmBackend;
class DrmAbstractOutput;
class DrmQPainterLayer;
class DrmPipeline;

class DrmQPainterBackend : public QPainterBackend, public DrmRenderBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend);
    ~DrmQPainterBackend();

    QImage *bufferForScreen(AbstractOutput *output) override;
    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    QSharedPointer<DrmPipelineLayer> createDrmPipelineLayer(DrmPipeline *pipeline) override;
    QSharedPointer<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

Q_SIGNALS:
    void aboutToBeDestroyed();

private:
    DrmBackend *m_backend;
};
}
