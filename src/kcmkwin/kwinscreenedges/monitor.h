/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@suse.cz>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    void setEdgeEnabled(int edge, bool enabled);
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
        BottomRight,
        None
    };
Q_SIGNALS:
    void changed();
    void edgeSelectionChanged(int edge, int index);
protected:
    void resizeEvent(QResizeEvent* e) override;
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
    ~Corner() override;
    void setActive(bool active);
    bool active() const;
protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* e) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent * e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent * e) override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
private:
    Monitor* monitor;
    Plasma::FrameSvg *button;
    bool m_active;
    bool m_hover;
};

} // namespace

#endif
