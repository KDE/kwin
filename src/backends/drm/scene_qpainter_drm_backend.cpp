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
    connect(m_backend, &DrmBackend::outputEnabled, this, [this] (const auto output) {
        m_swapchains[output] = QSharedPointer<DrmQPainterLayer>::create(static_cast<DrmAbstractOutput*>(output));
    });
    connect(m_backend, &DrmBackend::outputDisabled, this, [this] (const auto output) {
        m_swapchains.remove(output);
    });
}

QImage *DrmQPainterBackend::bufferForScreen(AbstractOutput *output)
{
    return static_cast<DrmDumbBuffer*>(m_swapchains[output]->currentBuffer().data())->image();
}

QRegion DrmQPainterBackend::beginFrame(AbstractOutput *output)
{
    return m_swapchains[output]->startRendering().value_or(QRegion());
}

void DrmQPainterBackend::endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damage)
{
    Q_UNUSED(renderedRegion)
    m_swapchains[output]->endRendering(damage);
    static_cast<DrmAbstractOutput*>(output)->present(m_swapchains[output]->currentBuffer(), output->geometry());
}

}
