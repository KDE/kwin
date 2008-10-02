/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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
#include <QObject>

namespace KWin
{

class DesktopGridEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        DesktopGridEffect();
        ~DesktopGridEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void windowClosed( EffectWindow* w );
        virtual void windowInputMouseEvent( Window w, QEvent* e );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        virtual bool borderActivated( ElectricBorder border );

        enum { LayoutPager, LayoutAutomatic, LayoutCustom }; // Layout modes

    private slots:
        void toggle();

    private:
        QPointF scalePos( const QPoint& pos, int desktop, int screen = -1 ) const;
        QPoint unscalePos( const QPoint& pos, int* desktop = NULL ) const;
        int posToDesktop( const QPoint& pos ) const;
        EffectWindow* windowAt( QPoint pos ) const;
        void setCurrentDesktop( int desktop );
        void setHighlightedDesktop( int desktop );
        void setActive( bool active );
        void setup();
        void finish();
        
        ElectricBorder borderActivate;
        int zoomDuration;
        int border;
        Qt::Alignment desktopNameAlignment;
        int layoutMode;
        int customLayoutRows;
        
        bool activated;
        TimeLine timeline;
        int paintingDesktop;
        int highlightedDesktop;
        Window input;
        bool keyboardGrab;
        bool wasWindowMove;
        EffectWindow* windowMove;
        QPoint windowMoveDiff;
        
        // Soft highlighting
        QList<TimeLine> hoverTimeline;
        
        QSize gridSize;
        Qt::Orientation orientation;
        QPoint activeCell;
        // Per screen variables
        QList<double> scale; // Because the border isn't a ratio each screen is different
        QList<double> unscaledBorder;
        QList<QSizeF> scaledSize;
        QList<QPointF> scaledOffset;

    };

} // namespace

#endif
