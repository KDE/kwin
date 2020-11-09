/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_drm_backend.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "logind.h"
#include "drm_gpu.h"

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
    connect(m_gpu, &DrmGpu::outputAdded, this, &DrmQPainterBackend::initOutput);
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
            delete (*it).buffer[0];
            delete (*it).buffer[1];
            m_outputs.erase(it);
        }
    );
}

DrmQPainterBackend::~DrmQPainterBackend()
{
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        delete (*it).buffer[0];
        delete (*it).buffer[1];
    }
}

void DrmQPainterBackend::initOutput(DrmOutput *output)
{
    Output o;
    auto initBuffer = [&o, output, this] (int index) {
        o.buffer[index] = m_gpu->createBuffer(output->pixelSize());
        if (o.buffer[index]->map()) {
            o.buffer[index]->image()->fill(Qt::black);
        }
    };
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
            delete (*it).buffer[0];
            delete (*it).buffer[1];
            auto initBuffer = [it, output, this] (int index) {
                it->buffer[index] = m_gpu->createBuffer(output->pixelSize());
                if (it->buffer[index]->map()) {
                    it->buffer[index]->image()->fill(Qt::black);
                }
            };
            initBuffer(0);
            initBuffer(1);
        }
    );
    initBuffer(0);
    initBuffer(1);
    o.output = output;
    m_outputs << o;
}

QImage *DrmQPainterBackend::bufferForScreen(int screenId)
{
    const Output &o = m_outputs.at(screenId);
    return o.buffer[o.index]->image();
}

bool DrmQPainterBackend::needsFullRepaint(int screenId) const
{
    Q_UNUSED(screenId)
    return true;
}

void DrmQPainterBackend::beginFrame(int screenId)
{
    Output &rendererOutput = m_outputs[screenId];
    rendererOutput.index = (rendererOutput.index + 1) % 2;
}

void DrmQPainterBackend::endFrame(int screenId, int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (!LogindIntegration::self()->isActiveSession()) {
        return;
    }

    const Output &rendererOutput = m_outputs[screenId];
    m_backend->present(rendererOutput.buffer[rendererOutput.index], rendererOutput.output);
}

bool DrmQPainterBackend::perScreenRendering() const
{
    return true;
}

}
