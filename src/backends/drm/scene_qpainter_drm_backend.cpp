/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_drm_backend.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_qpainter_layer.h"
#include "drm_virtual_output.h"
#include "renderloop_p.h"

#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    m_backend->setRenderBackend(this);
}

DrmQPainterBackend::~DrmQPainterBackend()
{
    m_backend->releaseBuffers();
    m_backend->setRenderBackend(nullptr);
}

void DrmQPainterBackend::present(Output *output)
{
    static_cast<DrmAbstractOutput *>(output)->present();
}

OutputLayer *DrmQPainterBackend::primaryLayer(Output *output)
{
    return static_cast<DrmAbstractOutput *>(output)->outputLayer();
}

QSharedPointer<DrmPipelineLayer> DrmQPainterBackend::createPrimaryLayer(DrmPipeline *pipeline)
{
    if (pipeline->output()) {
        return QSharedPointer<DrmQPainterLayer>::create(pipeline);
    } else {
        return QSharedPointer<DrmLeaseQPainterLayer>::create(pipeline);
    }
}

QSharedPointer<DrmOverlayLayer> DrmQPainterBackend::createCursorLayer(DrmPipeline *pipeline)
{
    return QSharedPointer<DrmCursorQPainterLayer>::create(pipeline);
}

QSharedPointer<DrmOutputLayer> DrmQPainterBackend::createLayer(DrmVirtualOutput *output)
{
    return QSharedPointer<DrmVirtualQPainterLayer>::create(output);
}

}
