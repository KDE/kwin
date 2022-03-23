/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderoutput.h"
#include "abstract_output.h"

namespace KWin
{

RenderOutput::RenderOutput(AbstractOutput *output)
    : m_output(output)
{
}

RenderOutput::~RenderOutput() = default;

QSize RenderOutput::pixelSize() const
{
    return geometry().size() * platformOutput()->scale();
}

QRect RenderOutput::relativePixelGeometry() const
{
    if (m_output) {
        return QRect(QPoint(geometry().topLeft().x() * m_output->scale() / m_output->pixelSize().width(), geometry().topLeft().y() * m_output->scale() / m_output->pixelSize().height()), m_output->pixelSize());
    } else {
        return geometry();
    }
}

AbstractOutput *RenderOutput::platformOutput() const
{
    return m_output;
}

SimpleRenderOutput::SimpleRenderOutput(AbstractOutput *output, bool needsSoftwareCursor)
    : RenderOutput(output)
    , m_layer(new OutputLayer())
    , m_needsSoftwareCursor(needsSoftwareCursor)
{
    connect(output, &AbstractOutput::geometryChanged, this, &RenderOutput::geometryChanged);
}

OutputLayer *SimpleRenderOutput::layer() const
{
    return m_layer.get();
}

bool SimpleRenderOutput::usesSoftwareCursor() const
{
    return m_needsSoftwareCursor;
}

QRect SimpleRenderOutput::geometry() const
{
    return m_output->geometry();
}
}
