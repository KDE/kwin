/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_DESKTOPGRID_H
#define KWIN_DESKTOPGRID_H

#include <kwineffects.h>
#include <qobject.h>

namespace KWin
{

class DesktopGridEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        DesktopGridEffect();
        ~DesktopGridEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowClosed( EffectWindow* w );
        virtual void desktopChanged( int old );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        virtual bool borderActivated( ElectricBorder border );
    private slots:
        void toggle();
    private:
        QRect desktopRect( int desktop, bool scaled ) const;
        int posToDesktop( const QPoint& pos ) const;
        QRect windowRect( EffectWindow* w ) const; // returns always scaled
        EffectWindow* windowAt( const QPoint& pos, QRect* rect = NULL ) const;
        void setActive( bool active );
        void setup();
        void finish();
        void paintSlide( int mask, QRegion region, const ScreenPaintData& data );
        void paintScreenDesktop( int desktop, int mask, QRegion region, ScreenPaintData data );
        void slideDesktopChanged( int old );
        void setHighlightedDesktop( int desktop );
        double progress;
        bool activated;
        int painting_desktop;
        int highlighted_desktop;
        Window input;
        bool keyboard_grab;
        bool was_window_move;
        EffectWindow* window_move;
        QPoint window_move_diff;
        QPoint window_move_pos;
        bool slideEnabled;
        bool slide;
        QPoint slide_start_pos;
        bool slide_painting_sticky;
        QPoint slide_painting_diff;
        ElectricBorder borderActivate;
    };

} // namespace

#endif
