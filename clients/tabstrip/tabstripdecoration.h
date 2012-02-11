/********************************************************************
Tabstrip KWin window decoration
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef TABSTRIPDECORATION_H
#define TABSTRIPDECORATION_H

#include <kdecoration.h>
#include <kcommondecoration.h>
#include <QPaintEvent>
#include <kdebug.h>

class TabstripButton;

class TabstripDecoration : public KCommonDecorationUnstable
    {
    public:
        TabstripDecoration( KDecorationBridge *bridge, KDecorationFactory *factory );
        KCommonDecorationButton *createButton( ButtonType type );
        void init();
        QString visibleName() const;
        virtual int layoutMetric(LayoutMetric lm, bool respectWindowState = true, const KCommonDecorationButton *button = 0) const;
        virtual bool eventFilter( QObject* o, QEvent* e );
    private:
        void paintTab( QPainter &painter, const QRect &geom, ClientGroupItem &item, bool active );
        void paintEvent( QPaintEvent *e );
        bool mouseSingleClickEvent( QMouseEvent* e );
        bool mouseMoveEvent( QMouseEvent* e );
        bool mouseButtonPressEvent( QMouseEvent* e );
        bool mouseButtonReleaseEvent( QMouseEvent* e );
        bool dragMoveEvent( QDragMoveEvent* e );
        bool dragLeaveEvent( QDragLeaveEvent* e );
        bool dragEnterEvent( QDragEnterEvent* e );
        bool dropEvent( QDropEvent* e );
        int itemClicked( const QPoint &point, bool between = false );
        QList< TabstripButton* > closeButtons;
        QPoint click, release;
        int targetTab;
        bool click_in_progress, drag_in_progress;
        Qt::MouseButton button;
    };

#endif
