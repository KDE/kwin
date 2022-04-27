/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderoutput.h"
#include "output.h"

namespace KWin
{

QSize RenderOutput::pixelSize() const
{
    return geometry().size() * platformOutput()->scale();
}

QRect RenderOutput::rect() const
{
    return QRect(QPoint(), geometry().size());
}

bool RenderOutput::usesSoftwareCursor() const
{
    return true;
}

QRect RenderOutput::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(-geometry().topLeft());
}

SimpleRenderOutput::SimpleRenderOutput(Output *output)
    : m_output(output)
{
    connect(output, &Output::geometryChanged, this, &RenderOutput::geometryChanged);
}

QRect SimpleRenderOutput::geometry() const
{
    return m_output->geometry();
}

Output *SimpleRenderOutput::platformOutput() const
{
    return m_output;
}

bool SimpleRenderOutput::usesSoftwareCursor() const
{
    return m_output->usesSoftwareCursor();
}
}
