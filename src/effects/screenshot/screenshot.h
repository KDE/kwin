/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCREENSHOT_H
#define KWIN_SCREENSHOT_H

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
struct ScreenShotWindowData;
struct ScreenShotAreaData;
struct ScreenShotScreenData;

/**
 * The screenshot effet allows to takes screenshot, by window, area, screen, etc...
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
    void postPaintScreen() override;
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

    void grabPointerImage(QImage& snapshot, int offsetx, int offsety);
    QImage blitScreenshot(const QRect &geometry, const qreal scale = 1.0);

    QVector<ScreenShotWindowData> m_windowScreenShots;
    QVector<ScreenShotAreaData> m_areaScreenShots;
    QVector<ScreenShotScreenData> m_screenScreenShots;

    QScopedPointer<ScreenShotDBusInterface1> m_dbusInterface1;
    EffectScreen *m_paintedScreen = nullptr;
};

} // namespace

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScreenShotFlags)

#endif // KWIN_SCREENSHOT_H
