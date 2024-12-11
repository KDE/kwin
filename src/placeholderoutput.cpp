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

    m_renderLoop = std::make_unique<RenderLoop>(this);
    m_renderLoop->setRefreshRate(mode->refreshRate());
    m_renderLoop->inhibit();

    m_nextState.scale = scale;
    m_nextState.modes = {mode};
    m_nextState.currentMode = mode;
    m_nextState.enabled = true;
    applyNextState();

    setInformation(Information{
        .name = QStringLiteral("Placeholder-1"),
        .placeholder = true,
    });
}

PlaceholderOutput::~PlaceholderOutput()
{
    m_nextState.enabled = false;
    applyNextState();
}

RenderLoop *PlaceholderOutput::renderLoop() const
{
    return m_renderLoop.get();
}

} // namespace KWin

#include "moc_placeholderoutput.cpp"
