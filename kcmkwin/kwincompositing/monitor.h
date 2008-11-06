/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@suse.cz>

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

#ifndef CCSM_MONITOR_H
#define CCSM_MONITOR_H

#include <qactiongroup.h>
#include <qgraphicsitem.h>
#include <qlabel.h>
#include <qvector.h>

class QAction;
class QGraphicsView;
class QGraphicsScene;
class QMenu;

namespace KWin
{

class Monitor
    : public QLabel
    {
    Q_OBJECT
    public:
        Monitor( QWidget* parent );
        void setEdge( int edge, bool set );
        bool edge( int edge ) const;
        void clear();
        void addEdgeItem( int edge, const QString& item );
        void selectEdgeItem( int edge, int index );
        int selectedEdgeItem( int edge ) const;

        enum Edges
            {
            Left,
            Right,
            Top,
            Bottom,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
            };
    signals:
        void changed();
        void edgeSelectionChanged( int edge, int index );
    protected:
        virtual void resizeEvent( QResizeEvent* e );
    private:
        class Corner;
        void popup( Corner* c, QPoint pos );
        void flip( Corner* c, QPoint pos );
        void checkSize();
        QGraphicsView* view;
        QGraphicsScene* scene;
        QGraphicsRectItem* items[ 8 ];
        QMenu* popups[ 8 ];
        QVector< QAction* > popup_actions[ 8 ];
        QActionGroup* grp[ 8 ];
    };

class Monitor::Corner
    : public QGraphicsRectItem
    {
    public:
        Corner( Monitor* m );
    protected:
        virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* e );
        virtual void mousePressEvent( QGraphicsSceneMouseEvent* e );
    private:
        Monitor* monitor;
    };

} // namespace

#endif
