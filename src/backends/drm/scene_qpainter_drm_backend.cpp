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
#include "renderoutput.h"

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
    m_backend->setRenderBackend(nullptr);
}

QImage *DrmQPainterBackend::bufferForScreen(RenderOutput *output)
{
    return dynamic_cast<QPainterLayer *>(output->layer())->image();
}

QRegion DrmQPainterBackend::beginFrame(RenderOutput *output)
{
    return static_cast<DrmOutputLayer *>(output->layer())->startRendering().value_or(QRegion());
}

void DrmQPainterBackend::endFrame(RenderOutput *output, const QRegion &renderedRegion, const QRegion &damage)
{
    Q_UNUSED(renderedRegion)
    static_cast<DrmOutputLayer *>(output->layer())->endRendering(damage);
}

void DrmQPainterBackend::present(AbstractOutput *output)
{
    static_cast<DrmAbstractOutput *>(output)->present();
}

QSharedPointer<DrmPipelineLayer> DrmQPainterBackend::createDrmPipelineLayer(DrmPipeline *pipeline)
{
    if (pipeline->output()) {
        return QSharedPointer<DrmQPainterLayer>::create(this, pipeline);
    } else {
        return QSharedPointer<DrmLeaseQPainterLayer>::create(this, pipeline);
    }
}

QSharedPointer<DrmOutputLayer> DrmQPainterBackend::createLayer(DrmVirtualOutput *output)
{
    return QSharedPointer<DrmVirtualQPainterLayer>::create(output);
}

}
