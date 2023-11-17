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

    /**
     * Schedules a screenshot of the given @a screen. The returned QFuture can be used to query
     * the image data. If the screen is removed before the screenshot is taken, the future will
     * be cancelled.
     */
    QFuture<QImage> scheduleScreenShot(Output *screen, ScreenShotFlags flags = {});

    /**
     * Schedules a screenshot of the given @a area. The returned QFuture can be used to query the
     * image data.
     */
    QFuture<QImage> scheduleScreenShot(const QRect &area, ScreenShotFlags flags = {});

    /**
     * Schedules a screenshot of the given @a window. The returned QFuture can be used to query
     * the image data. If the window is removed before the screenshot is taken, the future will
     * be cancelled.
     */
    QFuture<QImage> scheduleScreenShot(EffectWindow *window, ScreenShotFlags flags = {});

    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, Output *screen) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

private Q_SLOTS:
    void handleWindowClosed(EffectWindow *window);
    void handleScreenAdded();
    void handleScreenRemoved(Output *screen);

private:
    void takeScreenShot(ScreenShotWindowData *screenshot);
    bool takeScreenShot(const RenderTarget &renderTarget, const RenderViewport &viewport, ScreenShotAreaData *screenshot);
    bool takeScreenShot(const RenderTarget &renderTarget, const RenderViewport &viewport, ScreenShotScreenData *screenshot);

    void cancelWindowScreenShots();
    void cancelAreaScreenShots();
    void cancelScreenScreenShots();

    void grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const;
    QImage blitScreenshot(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRect &geometry, qreal devicePixelRatio = 1.0) const;

    std::vector<ScreenShotWindowData> m_windowScreenShots;
    std::vector<ScreenShotAreaData> m_areaScreenShots;
    std::vector<ScreenShotScreenData> m_screenScreenShots;

    std::unique_ptr<ScreenShotDBusInterface2> m_dbusInterface2;
    Output *m_paintedScreen = nullptr;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenShotFlags)
