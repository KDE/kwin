/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SCREENSHOT_H
#define KWIN_SCREENSHOT_H

#include <kwineffects.h>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QImage>

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
    virtual ~ScreenShotEffect();
    void paintScreen(int mask, QRegion region, ScreenPaintData &data) override;
    virtual void postPaintScreen();
    virtual bool isActive() const;

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
     **/
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
     **/
    Q_SCRIPTABLE void interactive(QDBusUnixFileDescriptor fd, int mask = 0);
    Q_SCRIPTABLE void screenshotWindowUnderCursor(int mask = 0);
    /**
     * Saves a screenshot of all screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotFullscreen(bool captureCursor = false);
    /**
     * Saves a screenshot of the screen identified by @p screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param screen Number of screen as numbered by QDesktopWidget
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotScreen(int screen, bool captureCursor = false);
    /**
     * Saves a screenshot of the selected geometry into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param x Left upper x coord of region
     * @param y Left upper y coord of region
     * @param width Width of the region to screenshot
     * @param height Height of the region to screenshot
     * @param captureCursor Whether to include the cursor in the image
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotArea(int x, int y, int width, int height, bool captureCursor = false);

Q_SIGNALS:
    Q_SCRIPTABLE void screenshotCreated(qulonglong handle);

private Q_SLOTS:
    void windowClosed( KWin::EffectWindow* w );

private:
    void grabPointerImage(QImage& snapshot, int offsetx, int offsety);
    QImage blitScreenshot(const QRect &geometry);
    QString saveTempImage(const QImage &img);
    void sendReplyImage(const QImage &img);
    void showInfoMessage();
    void hideInfoMessage();
    EffectWindow *m_scheduledScreenshot;
    ScreenShotType m_type;
    QRect m_scheduledGeometry;
    QDBusConnection m_replyConnection;
    QDBusMessage m_replyMessage;
    QRect m_cachedOutputGeometry;
    QImage m_multipleOutputsImage;
    QRegion m_multipleOutputsRendered;
    bool m_captureCursor = false;
    enum class WindowMode {
        NoCapture,
        Xpixmap,
        File,
        FileDescriptor
    };
    WindowMode m_windowMode = WindowMode::NoCapture;
    int m_fd = -1;
    QScopedPointer<EffectFrame> m_infoFrame;
};

} // namespace

#endif // KWIN_SCREENSHOT_H
