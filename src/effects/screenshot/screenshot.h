/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>

#include <QFuture>
#include <QFutureInterface>
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
};
Q_DECLARE_FLAGS(ScreenShotFlags, ScreenShotFlag)

class ScreenShotDBusInterface1;
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
    QFuture<QImage> scheduleScreenShot(EffectScreen *screen, ScreenShotFlags flags = {});

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

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;

    static bool supported();

private Q_SLOTS:
    void handleWindowClosed(EffectWindow *window);
    void handleScreenAdded();
    void handleScreenRemoved(EffectScreen *screen);

private:
    void takeScreenShot(ScreenShotWindowData *screenshot);
    bool takeScreenShot(ScreenShotAreaData *screenshot);
    bool takeScreenShot(ScreenShotScreenData *screenshot);

    void cancelWindowScreenShots();
    void cancelAreaScreenShots();
    void cancelScreenScreenShots();

    void grabPointerImage(QImage &snapshot, int xOffset, int yOffset) const;
    QImage blitScreenshot(const QRect &geometry, qreal devicePixelRatio = 1.0) const;

    QVector<ScreenShotWindowData> m_windowScreenShots;
    QVector<ScreenShotAreaData> m_areaScreenShots;
    QVector<ScreenShotScreenData> m_screenScreenShots;

    std::unique_ptr<ScreenShotDBusInterface1> m_dbusInterface1;
    std::unique_ptr<ScreenShotDBusInterface2> m_dbusInterface2;
    EffectScreen *m_paintedScreen = nullptr;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenShotFlags)
