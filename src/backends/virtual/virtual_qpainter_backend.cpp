/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_qpainter_backend.h"
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

std::optional<OutputLayerBeginFrameInfo> VirtualQPainterLayer::beginFrame()
{
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(&m_image),
        .repaint = m_output->rect(),
    };
}

bool VirtualQPainterLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return true;
}

QImage *VirtualQPainterLayer::image()
{
    return &m_image;
}

VirtualQPainterBackend::VirtualQPainterBackend(VirtualBackend *backend)
{
    connect(backend, &VirtualBackend::outputAdded, this, &VirtualQPainterBackend::addOutput);
    connect(backend, &VirtualBackend::outputRemoved, this, &VirtualQPainterBackend::removeOutput);

    const auto outputs = backend->outputs();
    for (Output *output : outputs) {
        addOutput(output);
    }
}

VirtualQPainterBackend::~VirtualQPainterBackend() = default;

void VirtualQPainterBackend::addOutput(Output *output)
{
    m_outputs[output] = std::make_unique<VirtualQPainterLayer>(output);
}

void VirtualQPainterBackend::removeOutput(Output *output)
{
    m_outputs.erase(output);
}

void VirtualQPainterBackend::present(Output *output)
{
    static_cast<VirtualOutput *>(output)->vsyncMonitor()->arm();
}

VirtualQPainterLayer *VirtualQPainterBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}
}
