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
#include "renderloop_p.h"

namespace KWin
{

DrmQPainterBackend::DrmQPainterBackend(DrmBackend *backend, DrmGpu *gpu)
    : QPainterBackend()
    , m_backend(backend)
    , m_gpu(gpu)
{
    const auto outputs = m_backend->drmOutputs();
    for (auto output: outputs) {
        initOutput(output);
    }
    connect(m_gpu, &DrmGpu::outputEnabled, this, &DrmQPainterBackend::initOutput);
    connect(m_gpu, &DrmGpu::outputDisabled, this,
        [this] (DrmOutput *o) {
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

void DrmQPainterBackend::initOutput(DrmOutput *output)
{
    Output o;
    o.swapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, output->pixelSize());
    o.output = output;
    m_outputs << o;
    connect(output, &DrmOutput::modeChanged, this,
        [output, this] {
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [output] (const auto &o) {
                    return o.output == output;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            it->swapchain = QSharedPointer<DumbSwapchain>::create(m_gpu, output->pixelSize());
            it->damageJournal.setCapacity(it->swapchain->slotCount());
        }
    );
}

QImage *DrmQPainterBackend::bufferForScreen(int screenId)
{
    return m_outputs[screenId].swapchain->currentBuffer()->image();
}

QRegion DrmQPainterBackend::beginFrame(int screenId)
{
    Output *rendererOutput = &m_outputs[screenId];

    int bufferAge;
    rendererOutput->swapchain->acquireBuffer(&bufferAge);

    return rendererOutput->damageJournal.accumulate(bufferAge, rendererOutput->output->geometry());
}

void DrmQPainterBackend::endFrame(int screenId, const QRegion &damage)
{
    Output &rendererOutput = m_outputs[screenId];
    DrmOutput *drmOutput = rendererOutput.output;

    QSharedPointer<DrmDumbBuffer> back = rendererOutput.swapchain->currentBuffer();
    rendererOutput.swapchain->releaseBuffer(back);

    if (!drmOutput->present(back, drmOutput->geometry())) {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(drmOutput->renderLoop());
        renderLoopPrivate->notifyFrameFailed();
    }

    rendererOutput.damageJournal.add(damage);
}

}
