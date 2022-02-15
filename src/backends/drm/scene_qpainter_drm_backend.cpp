/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_drm_backend.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "drm_gpu.h"
#include "drm_buffer.h"
#include "renderloop_p.h"
#include "drm_qpainter_layer.h"

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

QImage *DrmQPainterBackend::bufferForScreen(AbstractOutput *output)
{
    const auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    return static_cast<DrmDumbBuffer*>(drmOutput->outputLayer()->currentBuffer().data())->image();
}

QRegion DrmQPainterBackend::beginFrame(AbstractOutput *output)
{
    const auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    return drmOutput->outputLayer()->startRendering().value_or(QRegion());
}

void DrmQPainterBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damage)
{
    Q_UNUSED(renderedRegion)
    const auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    drmOutput->outputLayer()->endRendering(damage);
    static_cast<DrmAbstractOutput*>(output)->present();
}

QSharedPointer<DrmLayer> DrmQPainterBackend::createLayer(DrmDisplayDevice *displayDevice)
{
    return QSharedPointer<DrmQPainterLayer>::create(displayDevice);
}

}
