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
    State state = m_state;
    state.enabled = false;
    setState(state);
}

RenderLoop *PlaceholderOutput::renderLoop() const
{
    return m_renderLoop.get();
}

bool PlaceholderOutput::testPresentation(const std::shared_ptr<OutputFrame> &frame)
{
    return false;
}

bool PlaceholderOutput::present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame)
{
    return false;
}

} // namespace KWin

#include "moc_placeholderoutput.cpp"
