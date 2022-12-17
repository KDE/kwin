/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@suse.cz>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screenpreviewwidget.h"

#include <QActionGroup>
#include <QGraphicsItem>
#include <QVector>
#include <array>
#include <memory>

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

class Monitor : public ScreenPreviewWidget
{
    Q_OBJECT
public:
    explicit Monitor(QWidget *parent);
    ~Monitor();

    void setEdgeEnabled(int edge, bool enabled);
    void setEdgeHidden(int edge, bool set);
    bool edgeHidden(int edge) const;
    void clear();
    void addEdgeItem(int edge, const QString &item);
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
    void resizeEvent(QResizeEvent *e) override;
    bool event(QEvent *event) override;

private:
    class Corner;
    void popup(Corner *c, QPoint pos);
    void flip(Corner *c, QPoint pos);
    void checkSize();
    std::unique_ptr<QGraphicsScene> m_scene;
    std::unique_ptr<QGraphicsView> m_view;
    std::array<std::unique_ptr<Corner>, 8> m_items;
    std::array<bool, 8> m_hidden;
    std::array<std::unique_ptr<QMenu>, 8> m_popups;
    std::array<QVector<QAction *>, 8> m_popupActions;
    std::array<std::unique_ptr<QActionGroup>, 8> m_actionGroups;
};

class Monitor::Corner : public QGraphicsRectItem
{
public:
    Corner(Monitor *m);
    ~Corner() override;
    void setActive(bool active);
    bool active() const;

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    Monitor *const m_monitor;
    const std::unique_ptr<Plasma::FrameSvg> m_button;
    bool m_active = false;
    bool m_hover = false;
};

} // namespace
