/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "placeholderoutput.h"

namespace KWin
{

PlaceholderOutput::PlaceholderOutput(const QSize &size, qreal scale)
{
    auto mode = std::make_shared<OutputMode>(size, 60000);

    m_renderLoop = std::make_unique<RenderLoop>();
    m_renderLoop->setRefreshRate(mode->refreshRate());
    m_renderLoop->inhibit();

    setState(State{
        .scale = scale,
        .modes = {mode},
        .currentMode = mode,
        .enabled = true,
    });

    setInformation(Information{
        .name = QStringLiteral("Placeholder-1"),
        .placeholder = true,
    });
}

PlaceholderOutput::~PlaceholderOutput()
{
}

RenderLoop *PlaceholderOutput::renderLoop() const
{
    return m_renderLoop.get();
}

} // namespace KWin
