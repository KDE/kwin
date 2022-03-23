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

QRect RenderOutput::geometry() const
{
    return m_output->geometry();
}

QSize RenderOutput::pixelSize() const
{
    return geometry().size() * platformOutput()->scale();
}

AbstractOutput *RenderOutput::platformOutput() const
{
    return m_output;
}

SimpleRenderOutput::SimpleRenderOutput(AbstractOutput *output)
    : RenderOutput(output)
    , m_layer(new OutputLayer())
{
    connect(output, &AbstractOutput::geometryChanged, this, &RenderOutput::geometryChanged);
}

OutputLayer *SimpleRenderOutput::layer() const
{
    return m_layer.get();
}

bool SimpleRenderOutput::usesSoftwareCursor() const
{
    return m_output->usesSoftwareCursor();
}
}
