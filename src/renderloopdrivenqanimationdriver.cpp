/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderloopdrivenqanimationdriver.h"

namespace KWin
{

RenderLoopDrivenQAnimationDriver::RenderLoopDrivenQAnimationDriver(QObject *parent)
    : QAnimationDriver(parent)
{
}

void RenderLoopDrivenQAnimationDriver::advanceToNextFrame(std::chrono::nanoseconds nextFramePresentationTime)
{
    Q_ASSERT(isRunning());
    m_nextTime = nextFramePresentationTime;
    advance();
}

void RenderLoopDrivenQAnimationDriver::start()
{
    m_offset = std::chrono::steady_clock::now().time_since_epoch();
    m_nextTime = m_offset;
    QAnimationDriver::start();
}

void RenderLoopDrivenQAnimationDriver::stop()
{
    m_offset = {};
    m_nextTime = {};
    QAnimationDriver::stop();
}

qint64 RenderLoopDrivenQAnimationDriver::elapsed() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(m_nextTime - m_offset).count();
}

} // namespace KWin
