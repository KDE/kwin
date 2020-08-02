/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SCREENSHOT_H
#define KWIN_SCREENSHOT_H

#include <kwineffects.h>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QImage>

class ComparableQPoint;
namespace KWin
{

class ScreenShotEffect : public Effect, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Screenshot")
public:
    enum ScreenShotType {
        INCLUDE_DECORATION = 1 << 0,
        INCLUDE_CURSOR = 1 << 1
    };
    ScreenShotEffect();
    ~ScreenShotEffect() override;

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    static bool supported();
    static void convertFromGLImage(QImage &img, int w, int h);
public Q_SLOTS:
    Q_SCRIPTABLE void screenshotForWindow(qulonglong winid, int mask = 0);
    /**
     * Starts an interactive window screenshot session. The user can select a window to
     * screenshot.
     *
     * Once the window is selected the screenshot is saved into a file and the path gets
     * returned to the DBus peer.
     *
     * @param mask The mask for what to include in the screenshot
     */
    Q_SCRIPTABLE QString interactive(int mask = 0);
    /**
     * Starts an interactive window screenshot session. The user can select a window to
     * screenshot.
     *
     * Once the window is selected the screenshot is saved into the @p fd passed to the
     * method. It is intended to be used with a pipe, so that the invoking side can just
     * read from the pipe. The image gets written into the fd using a QDataStream.
     *
     * @param fd File descriptor into which the screenshot should be saved
     * @param mask The mask for what to include in the screenshot
     */
    Q_SCRIPTABLE void interactive(QDBusUnixFileDescriptor fd, int mask = 0);
    Q_SCRIPTABLE void screenshotWindowUnderCursor(int mask = 0);
    /**
     * Saves a screenshot of all screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     */
    Q_SCRIPTABLE QString screenshotFullscreen(bool captureCursor = false);
    /**
     * Starts an interactive screenshot session.
     *
     * The user is asked to confirm that a screenshot is taken by having to actively
     * click and giving the possibility to cancel.
     *
     * Once the screenshot is taken it gets saved into the @p fd passed to the
     * method. It is intended to be used with a pipe, so that the invoking side can just
     * read from the pipe. The image gets written into the fd using a QDataStream.
     *
     * @param fd File descriptor into which the screenshot should be saved
     * @param captureCursor Whether to include the mouse cursor
     * @param shouldReturnNativeSize Whether to return an image according to the virtualGeometry, or according to pixel on screen size
     */
    Q_SCRIPTABLE void screenshotFullscreen(QDBusUnixFileDescriptor fd, bool captureCursor = false, bool shouldReturnNativeSize = false);
    /**
     * Saves a screenshot of the screen identified by @p screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param screen Number of screen as numbered by QDesktopWidget
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     */
    Q_SCRIPTABLE QString screenshotScreen(int screen, bool captureCursor = false);
    /**
     * Starts an interactive screenshot of a screen session.
     *
     * The user is asked to select the screen to screenshot.
     *
     * Once the screenshot is taken it gets saved into the @p fd passed to the
     * method. It is intended to be used with a pipe, so that the invoking side can just
     * read from the pipe. The image gets written into the fd using a QDataStream.
     *
     * @param fd File descriptor into which the screenshot should be saved
     * @param captureCursor Whether to include the mouse cursor
     */
    Q_SCRIPTABLE void screenshotScreen(QDBusUnixFileDescriptor fd, bool captureCursor = false);
    /**
     * Saves a screenshot of the selected geometry into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param x Left upper x coord of region
     * @param y Left upper y coord of region
     * @param width Width of the region to screenshot
     * @param height Height of the region to screenshot
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     */
    Q_SCRIPTABLE QString screenshotArea(int x, int y, int width, int height, bool captureCursor = false);

Q_SIGNALS:
    Q_SCRIPTABLE void screenshotCreated(qulonglong handle);

private Q_SLOTS:
    void windowClosed( KWin::EffectWindow* w );

private:
    void grabPointerImage(QImage& snapshot, int offsetx, int offsety);
    QImage blitScreenshot(const QRect &geometry, const qreal scale = 1.0);
    QString saveTempImage(const QImage &img);
    void sendReplyImage(const QImage &img);
    enum class InfoMessageMode {
        Window,
        Screen
    };
    void showInfoMessage(InfoMessageMode mode);
    void hideInfoMessage();
    bool isTakingScreenshot() const;
    void computeCoordinatesAfterScaling();
    bool checkCall() const;

    EffectWindow *m_scheduledScreenshot;
    ScreenShotType m_type;
    QRect m_scheduledGeometry;
    QDBusMessage m_replyMessage;
    QRect m_cachedOutputGeometry;
    QRegion m_multipleOutputsRendered;
    QMap<ComparableQPoint, QImage> m_cacheOutputsImages;
    bool m_captureCursor = false;
    bool m_nativeSize = false;
    enum class WindowMode {
        NoCapture,
        Xpixmap,
        File,
        FileDescriptor
    };
    WindowMode m_windowMode = WindowMode::NoCapture;
    int m_fd = -1;
    qreal m_cachedScale;
};

} // namespace

#endif // KWIN_SCREENSHOT_H
