/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_COVERSWITCH_H
#define KWIN_COVERSWITCH_H

#include <QHash>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QTimeLine>
#include <QQueue>

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class CoverSwitchEffect
    : public Effect
{
    Q_OBJECT
public:
    CoverSwitchEffect();
    ~CoverSwitchEffect();

    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual void windowInputMouseEvent(Window w, QEvent* e);
    virtual bool isActive() const;

    static bool supported();

public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow *c);
    void slotTabBoxAdded(int mode);
    void slotTabBoxClosed();
    void slotTabBoxUpdated();
    void slotTabBoxKeyEvent(QKeyEvent* event);

private:
    void paintScene(EffectWindow* frontWindow, const EffectWindowList& leftWindows, const EffectWindowList& rightWindows,
                    bool reflectedWindows = false);
    void paintWindowCover(EffectWindow* w, bool reflectedWindow, WindowPaintData& data);
    void paintFrontWindow(EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow);
    void paintWindows(const EffectWindowList& windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow = NULL);
    void abort();
    /**
     * Updates the caption of the caption frame.
     * Taking care of rewording the desktop client.
     * As well sets the icon for the caption frame.
     **/
    void updateCaption();

    bool mActivated;
    float angle;
    bool animateSwitch;
    bool animateStart;
    bool animateStop;
    bool animation;
    bool start;
    bool stop;
    bool reflection;
    float mirrorColor[2][4];
    bool windowTitle;
    int animationDuration;
    bool stopRequested;
    bool startRequested;
    QTimeLine timeLine;
    QRect area;
    Window input;
    float zPosition;
    float scaleFactor;
    enum Direction {
        Left,
        Right
    };
    Direction direction;
    QQueue<Direction> scheduled_directions;
    EffectWindow* selected_window;
    int activeScreen;
    QList< EffectWindow* > leftWindows;
    QList< EffectWindow* > rightWindows;
    EffectWindowList currentWindowList;
    EffectWindowList referrencedWindows;

    EffectFrame* captionFrame;
    QFont captionFont;

    bool thumbnails;
    bool dynamicThumbnails;
    int thumbnailWindows;

    bool primaryTabBox;
    bool secondaryTabBox;

    GLShader *m_reflectionShader;
};

} // namespace

#endif
