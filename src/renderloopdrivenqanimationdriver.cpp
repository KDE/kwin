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
    if (m_nextTime && m_nextTime >= nextFramePresentationTime) {
        return;
    }

    m_nextTime = nextFramePresentationTime;
    if (!m_offset) {
        m_offset = nextFramePresentationTime;
    }

    advance();
}

void RenderLoopDrivenQAnimationDriver::stop()
{
    m_offset.reset();
    m_nextTime.reset();
    QAnimationDriver::stop();
}

qint64 RenderLoopDrivenQAnimationDriver::elapsed() const
{
    if (!m_offset) {
        return 0;
    } else {
        return std::chrono::duration_cast<std::chrono::milliseconds>(*m_nextTime - *m_offset).count();
    }
}

} // namespace KWin
