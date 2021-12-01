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
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [o] (const Output &output) {
                    return output.output == o;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            m_outputs.erase(it);
        }
    );
}

void DrmQPainterBackend::initOutput(DrmAbstractOutput *output)
{
    Output o;
    o.swapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, output->sourceSize(), DRM_FORMAT_XRGB8888);
    o.output = output;
    m_outputs.insert(output, o);
    connect(output, &DrmOutput::currentModeChanged, this,
        [output, this] {
            auto &o = m_outputs[output];
            o.swapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, output->sourceSize(), DRM_FORMAT_XRGB8888);
            o.damageJournal.setCapacity(o.swapchain->slotCount());
        }
    );
}

QImage *DrmQPainterBackend::bufferForScreen(AbstractOutput *output)
{
    return m_outputs[output].swapchain->currentBuffer()->image();
}

QRegion DrmQPainterBackend::beginFrame(AbstractOutput *output)
{
    Output *rendererOutput = &m_outputs[output];

    int bufferAge;
    rendererOutput->swapchain->acquireBuffer(&bufferAge);

    return rendererOutput->damageJournal.accumulate(bufferAge, rendererOutput->output->geometry());
}

void DrmQPainterBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damage)
{
    Q_UNUSED(renderedRegion)
    Output &rendererOutput = m_outputs[output];
    DrmAbstractOutput *drmOutput = rendererOutput.output;

    QSharedPointer<DrmDumbBuffer> back = rendererOutput.swapchain->currentBuffer();
    rendererOutput.swapchain->releaseBuffer(back);

    drmOutput->present(back, drmOutput->geometry());

    rendererOutput.damageJournal.add(damage);
}

}
