/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>

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

#ifndef KWIN_BOXSWITCH_H
#define KWIN_BOXSWITCH_H

#include <kwineffects.h>

#include <QHash>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QTimeLine>
#include <QFont>
#include <QQueue>

#include <kwinglutils.h>
#include <kwinxrenderutils.h>

#include "boxswitch_proxy.h"

namespace KWin
{

class BoxSwitchEffect
    : public Effect
{
    Q_OBJECT
public:
    BoxSwitchEffect();
    ~BoxSwitchEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData &data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);

    virtual void windowInputMouseEvent(Window w, QEvent* e);
    virtual void* proxy();
    virtual bool isActive() const;
    void activateFromProxy(int mode, bool animate, bool showText, float positioningFactor);
    void paintWindowsBox(const QRegion& region);

public Q_SLOTS:
    void slotWindowClosed(KWin::EffectWindow* w);
    void slotTabBoxAdded(int mode);
    void slotTabBoxClosed();
    void slotTabBoxUpdated();
    void slotWindowGeometryShapeChanged(KWin::EffectWindow *w, const QRect &old);
    void slotWindowDamaged(KWin::EffectWindow* w, const QRect& damage);

private:
    class ItemInfo;
    void setActive();
    void setInactive();
    void moveResizeInputWindow(int x, int y, int width, int height);
    void calculateFrameSize();
    void calculateItemSizes();
    void setSelectedWindow(EffectWindow* w);

    void paintWindowThumbnail(EffectWindow* w);
    void paintDesktopThumbnail(int iDesktop);
    void paintWindowIcon(EffectWindow* w);

    bool mActivated;
    Window mInput;
    int mMode;

    EffectFrame* thumbnailFrame;

    QRect frame_area;
    int highlight_margin;
    QSize item_max_size; // maximum item display size (including highlight)
    QRect text_area;
    QFont text_font;
    QColor color_frame;
    QColor color_highlight;

    float bg_opacity;
    bool elevate_window;

    QHash< EffectWindow*, ItemInfo* > windows;
    EffectWindowList original_windows;
    EffectWindowList referrencedWindows;
    EffectWindow* selected_window;
    QHash< int, ItemInfo* > desktops;
    QList< int > original_desktops;
    int selected_desktop;

    int painting_desktop;

    bool mAnimateSwitch;
    QTimeLine activeTimeLine;
    QTimeLine timeLine;
    bool animation;
    QRect highlight_area;
    bool highlight_is_set;
    enum Direction {
        Left,
        Right
    };
    Direction direction;
    QQueue<Direction> scheduled_directions;
    EffectWindow* edge_window;
    EffectWindow* right_window;

    bool primaryTabBox;
    bool secondaryTabBox;

    BoxSwitchEffectProxy mProxy;
    bool mProxyActivated;
    bool mProxyAnimateSwitch;
    bool mProxyShowText;
    float mPositioningFactor;
};

class BoxSwitchEffect::ItemInfo
{
public:
    ItemInfo();
    ~ItemInfo();
    QRect area; // maximal painting area, including any frames/highlights/etc.
    QRegion clickable;
    QRect thumbnail;
    EffectFrame* iconFrame;
};

} // namespace

#endif
