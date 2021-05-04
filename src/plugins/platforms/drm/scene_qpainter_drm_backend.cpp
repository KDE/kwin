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
    : QObject()
    , QPainterBackend()
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
        }
    );
}

QImage *DrmQPainterBackend::bufferForScreen(int screenId)
{
    return m_outputs[screenId].swapchain->currentBuffer()->image();
}

bool DrmQPainterBackend::needsFullRepaint(int screenId) const
{
    Q_UNUSED(screenId)
    return true;
}

void DrmQPainterBackend::beginFrame(int screenId)
{
    m_outputs[screenId].swapchain->acquireBuffer();
}

void DrmQPainterBackend::endFrame(int screenId, int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)

    const Output &rendererOutput = m_outputs[screenId];
    DrmOutput *drmOutput = rendererOutput.output;

    if (!drmOutput->present(rendererOutput.swapchain->currentBuffer())) {
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(drmOutput->renderLoop());
        renderLoopPrivate->notifyFrameFailed();
    }
}

}
