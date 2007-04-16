/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
        virtual void prePaintScreen( int* mask, QRegion* region, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowClosed( EffectWindow* w );
        virtual void desktopChanged( int old );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
    private slots:
        void toggle();
    private:
        QRect desktopRect( int desktop, bool scaled ) const;
        int posToDesktop( const QPoint& pos ) const;
        QRect windowRect( EffectWindow* w ) const; // returns always scaled
        void setActive( bool active );
        void setup();
        void finish();
        void paintSlide( int mask, QRegion region, const ScreenPaintData& data );
        void slideDesktopChanged( int old );
        float progress;
        bool activated;
        int painting_desktop;
        int hover_desktop;
        Window input;
        bool keyboard_grab;
        bool was_window_move;
        EffectWindow* window_move;
        QPoint window_move_diff;
        QPoint window_move_pos;
        bool slide;
        QPoint slide_start_pos;
        bool slide_painting_sticky;
        QPoint slide_painting_diff;
    };

} // namespace

#endif
