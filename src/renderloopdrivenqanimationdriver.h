/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 KWin contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAnimationDriver>
#include <chrono>

namespace KWin
{

/**
 * The RenderLoopDrivenQAnimationDriver class
 * allowing kwin to control when Qt's internal animations update
 * to the next position and the timestamp they are targeting.
 */
class RenderLoopDrivenQAnimationDriver : public QAnimationDriver
{
public:
    explicit RenderLoopDrivenQAnimationDriver(QObject *parent = nullptr);

    void advanceToNextFrame(std::chrono::nanoseconds nextPresentation);

    /*
     * The overrides are for Qt's usage
     * we shouldn't need to call them from kwin code
     */
    void stop() override;
    qint64 elapsed() const override;

private:
    std::optional<std::chrono::nanoseconds> m_nextTime = std::nullopt;
    // the elapsed time is a relative offset to when the animationDriver starts
    std::optional<std::chrono::nanoseconds> m_offset = std::nullopt;
};

} // namespace KWin
