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

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    connect(m_backend, &DrmBackend::outputDisabled, this, [this] (const auto output) {
        m_swapchains.remove(output);
    });
}

QImage *DrmQPainterBackend::bufferForScreen(AbstractOutput *output)
{
    return m_swapchains[output]->currentBuffer()->image();
}

QRegion DrmQPainterBackend::beginFrame(AbstractOutput *output)
{
    const auto drmOutput = static_cast<DrmAbstractOutput*>(output);
    if (!m_swapchains[output] || m_swapchains[output]->size() != drmOutput->sourceSize()) {
        m_swapchains[output] = QSharedPointer<DumbSwapchain>::create(drmOutput->gpu(), drmOutput->sourceSize(), DRM_FORMAT_XRGB8888);
    }
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
