/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "drm_dumb_swapchain.h"
#include "drm_render_backend.h"
#include "qpainterbackend.h"

#include <QObject>
#include <QVector>

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

    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

    std::shared_ptr<DrmPipelineLayer> createPrimaryLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOverlayLayer> createCursorLayer(DrmPipeline *pipeline) override;
    std::shared_ptr<DrmOutputLayer> createLayer(DrmVirtualOutput *output) override;

private:
    DrmBackend *m_backend;
};
}
