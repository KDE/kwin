/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter_virtual_backend.h"
#include "cursor.h"
#include "softwarevsyncmonitor.h"
#include "virtual_backend.h"
#include "virtual_output.h"

#include <QPainter>

namespace KWin
{

VirtualQPainterLayer::VirtualQPainterLayer(Output *output)
    : m_output(output)
    , m_image(output->pixelSize(), QImage::Format_RGB32)
{
    m_image.fill(Qt::black);
}

OutputLayerBeginFrameInfo VirtualQPainterLayer::beginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = m_output->rect(),
    };
}

bool VirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)
    return true;
}

QImage *VirtualQPainterLayer::image()
{
    return &m_image;
}

VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
    : QPainterBackend()
    , m_backend(backend)
{
    connect(backend, &VirtualBackend::outputEnabled, this, &VirtualQPainterBackend::addOutput);
    connect(backend, &VirtualBackend::outputDisabled, this, &VirtualQPainterBackend::removeOutput);

    const auto outputs = backend->enabledOutputs();
    for (Output *output : outputs) {
        addOutput(output);
    }
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

void VirtualQPainterBackend::addOutput(Output *output)
{
    m_outputs.insert(output, std::make_shared<VirtualQPainterLayer>(output));
}

void VirtualQPainterBackend::removeOutput(Output *output)
{
    m_outputs.remove(output);
}

void VirtualQPainterBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();

    if (m_backend->saveFrames()) {
        m_outputs[output]->image()->save(QStringLiteral("%1/%s-%3.png").arg(m_backend->screenshotDirPath(), output->name(), QString::number(m_frameCounter++)));
    }
}

VirtualQPainterLayer *VirtualQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}
}
