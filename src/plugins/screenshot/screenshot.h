/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"

#include <QFuture>
#include <QImage>
#include <QObject>

namespace KWin
{

/**
 * This enum type is used to specify how a screenshot needs to be taken.
 */
enum ScreenShotFlag {
    ScreenShotIncludeDecoration = 0x1, ///< Include window titlebar and borders
    ScreenShotIncludeCursor = 0x2, ///< Include the cursor
    ScreenShotNativeResolution = 0x4, ///< Take the screenshot at the native resolution
    ScreenShotIncludeShadow = 0x8, ///< Include the window shadow
};
Q_DECLARE_FLAGS(ScreenShotFlags, ScreenShotFlag)

class ScreenShotDBusInterface2;
struct ScreenShotWindowData;
struct ScreenShotAreaData;
struct ScreenShotScreenData;

/**
 * The ScreenShotEffect provides a convenient way to capture the contents of a given window,
 * screen or an area in the global coordinates.
 *
 * Use the QFutureWatcher class to get notified when the requested screenshot is ready. Note
 * that the screenshot QFuture object can get cancelled if the captured window or the screen is
 * removed.
 */
class ScreenShotEffect : public Effect
{
    Q_OBJECT

public:
    ScreenShotEffect();
    ~ScreenShotEffect() override;

    std::optional<QImage> takeScreenShot(Output *screen, ScreenShotFlags flags = {});
    std::optional<QImage> takeScreenShot(const QRect &area, ScreenShotFlags flags = {});
    std::optional<QImage> takeScreenShot(EffectWindow *window, ScreenShotFlags flags = {});

    bool isActive() const override;

    static bool supported();

private:
    std::unique_ptr<ScreenShotDBusInterface2> m_dbusInterface2;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenShotFlags)
