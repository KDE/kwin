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
#include <QPixmap>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QQueue>

#include <kwineffects.h>
#include <kwinglutils.h>

namespace KWin
{

class CoverSwitchEffect
    : public Effect
    {
    public:
        CoverSwitchEffect();
        ~CoverSwitchEffect();

        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void tabBoxAdded( int mode );
        virtual void tabBoxClosed();
        virtual void tabBoxUpdated();
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void windowClosed( EffectWindow* c );

        static bool supported();
    private:
        void paintScene( EffectWindow* frontWindow, const EffectWindowList& leftWindows, const EffectWindowList& rightWindows,
        bool reflectedWindows = false );
        void paintWindowCover( EffectWindow* w, bool reflectedWindow, WindowPaintData& data );
        void paintFrontWindow( EffectWindow* frontWindow, int width, int leftWindows, int rightWindows, bool reflectedWindow  );
        void paintWindows( const EffectWindowList& windows, bool left, bool reflectedWindows, EffectWindow* additionalWindow = NULL );
        void abort();
        // thumbnail bar
        class ItemInfo;
        void calculateFrameSize();
        void calculateItemSizes();
        void paintFrame();
        void paintHighlight( QRect area );
        void paintWindowThumbnail( EffectWindow* w );
        void paintWindowIcon( EffectWindow* w );

        bool mActivated;
        float angle;
        bool animateSwitch;
        bool animateStart;
        bool animateStop;
        bool animation;
        bool start;
        bool stop;
        bool reflection;
        bool windowTitle;
        int animationDuration;
        bool stopRequested;
        bool startRequested;
        TimeLine timeLine;
        QRect area;
        Window input;
        float zPosition;
        float scaleFactor;
        enum Direction
            {
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

        EffectFrame captionFrame;
        QFont captionFont;

        // thumbnail bar
        EffectFrame thumbnailFrame;
        bool thumbnails;
        bool dynamicThumbnails;
        int thumbnailWindows;
        QHash< EffectWindow*, ItemInfo* > windows;
        QRect frame_area;
        int frame_margin;
        int highlight_margin;
        QSize item_max_size; // maximum item display size (including highlight)
        QColor color_frame;
        QColor color_highlight;
        QColor color_text;
        EffectWindow* edge_window;
        EffectWindow* right_window;
        QRect highlight_area;
        bool highlight_is_set;

    };

class CoverSwitchEffect::ItemInfo
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
