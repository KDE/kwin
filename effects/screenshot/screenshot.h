/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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
#include <QObject>
#include <QImage>

namespace KWin
{

class ScreenShotEffect : public Effect
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
    virtual void postPaintScreen();
    virtual bool isActive() const;

    static bool supported();
    static void convertFromGLImage(QImage &img, int w, int h);
public Q_SLOTS:
    Q_SCRIPTABLE void screenshotForWindow(qulonglong winid, int mask = 0);
    Q_SCRIPTABLE void screenshotWindowUnderCursor(int mask = 0);
    /**
     * Saves a screenshot of all screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotFullscreen();
    /**
     * Saves a screenshot of the screen identified by @p screen into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param screen Number of screen as numbered by QDesktopWidget
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotScreen(int screen);
    /**
     * Saves a screenshot of the selected geometry into a file and returns the path to the file.
     * Functionality requires hardware support, if not available a null string is returned.
     * @param x Left upper x coord of region
     * @param y Left upper y coord of region
     * @param width Width of the region to screenshot
     * @param height Height of the region to screenshot
     * @returns Path to stored screenshot, or null string in failure case.
     **/
    Q_SCRIPTABLE QString screenshotArea(int x, int y, int width, int height);

Q_SIGNALS:
    Q_SCRIPTABLE void screenshotCreated(qulonglong handle);

private slots:
    void windowClosed( KWin::EffectWindow* w );

private:
    void grabPointerImage(QImage& snapshot, int offsetx, int offsety);
    QString blitScreenshot(const QRect &geometry);
    EffectWindow *m_scheduledScreenshot;
    ScreenShotType m_type;
    QPixmap m_lastScreenshot;
};

} // namespace

#endif // KWIN_SCREENSHOT_H
