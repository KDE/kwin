/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lubos Lunak <l.lunak@suse.cz>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "screenpreviewwidget.h"

#include <QActionGroup>
#include <QGraphicsItem>
#include <QVector>

class QAction;
class QGraphicsView;
class QGraphicsScene;
class QMenu;

namespace Plasma
{
class FrameSvg;
}

namespace KWin
{

class Monitor
    : public ScreenPreviewWidget
{
    Q_OBJECT
public:
    explicit Monitor(QWidget* parent);
    void setEdge(int edge, bool set);
    bool edge(int edge) const;
    void setEdgeHidden(int edge, bool set);
    bool edgeHidden(int edge) const;
    void clear();
    void addEdgeItem(int edge, const QString& item);
    void setEdgeItemEnabled(int edge, int index, bool enabled);
    bool edgeItemEnabled(int edge, int index) const;
    void selectEdgeItem(int edge, int index);
    int selectedEdgeItem(int edge) const;

    enum Edges {
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };
Q_SIGNALS:
    void changed();
    void edgeSelectionChanged(int edge, int index);
protected:
    virtual void resizeEvent(QResizeEvent* e);
private:
    class Corner;
    void popup(Corner* c, QPoint pos);
    void flip(Corner* c, QPoint pos);
    void checkSize();
    QGraphicsView* view;
    QGraphicsScene* scene;
    Corner* items[ 8 ];
    bool hidden[ 8 ];
    QMenu* popups[ 8 ];
    QVector< QAction* > popup_actions[ 8 ];
    QActionGroup* grp[ 8 ];
};

class Monitor::Corner
    : public QGraphicsRectItem
{
public:
    Corner(Monitor* m);
    ~Corner();
    void setActive(bool active);
    bool active() const;
protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* e);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* e);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent * e);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent * e);
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = 0);
private:
    Monitor* monitor;
    Plasma::FrameSvg *button;
    bool m_active;
    bool m_hover;
};

} // namespace

#endif
