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

#include <drm_fourcc.h>

namespace KWin
{

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend, DrmGpu *gpu)
    : QPainterBackend()
    , m_backend(backend)
    , m_gpu(gpu)
{
    const auto outputs = m_backend->enabledOutputs();
    for (auto output: outputs) {
        initOutput(static_cast<DrmAbstractOutput*>(output));
    }
    connect(m_gpu, &DrmGpu::outputEnabled, this, &DrmQPainterBackend::initOutput);
    connect(m_gpu, &DrmGpu::outputDisabled, this,
        [this] (DrmAbstractOutput *o) {
            m_swapchains.remove(o);
        }
    );
}

void DrmQPainterBackend::initOutput(DrmAbstractOutput *output)
{
    m_swapchains.insert(output, QSharedPointer<DumbSwapchain>::create(m_gpu, output->sourceSize(), DRM_FORMAT_XRGB8888));
    connect(output, &DrmOutput::currentModeChanged, this,
        [output, this] {
            m_swapchains[output] = QSharedPointer<DumbSwapchain>::create(m_gpu, output->sourceSize(), DRM_FORMAT_XRGB8888);
        }
    );
}

QImage *DrmQPainterBackend::bufferForScreen(AbstractOutput *output)
{
    return m_swapchains[output]->currentBuffer()->image();
}

QRegion DrmQPainterBackend::beginFrame(AbstractOutput *output)
{
    QRegion needsRepainting;
    m_swapchains[output]->acquireBuffer(output->geometry(), &needsRepainting);
    return needsRepainting;
}

void DrmQPainterBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damage)
{
    Q_UNUSED(renderedRegion)
    QSharedPointer<DrmDumbBuffer> back = m_swapchains[output]->currentBuffer();
    m_swapchains[output]->releaseBuffer(back, damage);
    static_cast<DrmAbstractOutput*>(output)->present(back, output->geometry());
}

}
