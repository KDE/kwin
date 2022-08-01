/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screenshot.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>

namespace KWin
{

class ScreenShotSink1;
class ScreenShotSource1;

/**
 * The ScreenshotDBusInterface1 class provides a d-bus api to take screenshots. This implements
 * the org.kde.kwin.Screenshot interface.
 *
 * An application that requests a screenshot must have "org.kde.kwin.Screenshot" listed in its
 * X-KDE-DBUS-Restricted-Interfaces desktop file field.
 *
 * @deprecated Use org.kde.KWin.ScreenShot2 interface instead.
 */
class ScreenShotDBusInterface1 : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Screenshot")

public:
    explicit ScreenShotDBusInterface1(ScreenShotEffect *effect, QObject *parent = nullptr);
    ~ScreenShotDBusInterface1() override;

Q_SIGNALS:
    /**
     * This signal is emitted when a screenshot has been written in an Xpixmap with the
     * specified @a handle.
     */
    Q_SCRIPTABLE void screenshotCreated(qulonglong handle);

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
     * Takes a full screen screenshot in a one file format.
     *
     * Once the screenshot is taken it gets saved into the @p fd passed to the
     * method. It is intended to be used with a pipe, so that the invoking side can just
     * read from the pipe. The image gets written into the fd using a QDataStream.
     *
     * @param fd File descriptor into which the screenshot should be saved
     * @param captureCursor Whether to include the mouse cursor
     * @param shouldReturnNativeSize Whether to return an image according to the virtualGeometry,
     * or according to pixel on screen size
     */
    Q_SCRIPTABLE void screenshotFullscreen(QDBusUnixFileDescriptor fd,
                                           bool captureCursor = false,
                                           bool shouldReturnNativeSize = false);

    /**
     * Take a screenshot of the passed screens and return a QList<QImage> in the fd response,
     * an image for each screen in pixel-on-screen size when shouldReturnNativeSize is passed,
     * or converted to using logicale size if not
     *
     * @param fd
     * @param screensNames the names of the screens whose screenshot to return
     * @param captureCursor
     * @param shouldReturnNativeSize
     */
    Q_SCRIPTABLE void screenshotScreens(QDBusUnixFileDescriptor fd,
                                        const QStringList &screensNames,
                                        bool captureCursor = false,
                                        bool shouldReturnNativeSize = false);

    /**
     * Saves a screenshot of the screen identified by @p screen into a file and returns the path
     * to the file.
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

private Q_SLOTS:
    void handleSourceCompleted();
    void handleSourceCancelled();

private:
    enum class InfoMessageMode {
        Window,
        Screen,
    };

    void takeScreenShot(EffectScreen *screen, ScreenShotFlags flags, ScreenShotSink1 *sink);
    void takeScreenShot(const QList<EffectScreen *> &screens, ScreenShotFlags flags, ScreenShotSink1 *sink);
    void takeScreenShot(const QRect &area, ScreenShotFlags flags, ScreenShotSink1 *sink);
    void takeScreenShot(EffectWindow *window, ScreenShotFlags flags, ScreenShotSink1 *sink);

    void bind(ScreenShotSink1 *sink, ScreenShotSource1 *source);

    bool checkCall() const;
    bool isTakingScreenshot() const;

    void showInfoMessage(InfoMessageMode mode);
    void hideInfoMessage();

    ScreenShotEffect *m_effect;
    std::unique_ptr<ScreenShotSink1> m_sink;
    std::unique_ptr<ScreenShotSource1> m_source;
};

} // namespace KWin
