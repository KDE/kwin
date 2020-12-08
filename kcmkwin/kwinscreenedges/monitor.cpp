/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Lubos Lunak <l.lunak@suse.cz>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "monitor.h"

#include <KLocalizedString>
#include <Plasma/FrameSvg>

#include <QApplication>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QMenu>
#include <QScreen>
#include <QWindow>

namespace KWin
{

static QWindow *windowFromWidget(const QWidget *widget)
{
    QWindow *windowHandle = widget->windowHandle();
    if (windowHandle) {
        return windowHandle;
    }

    const QWidget *nativeParent = widget->nativeParentWidget();
    if (nativeParent) {
        return nativeParent->windowHandle();
    }

    return nullptr;
}

static QScreen *screenFromWidget(const QWidget *widget)
{
    const QWindow *windowHandle = windowFromWidget(widget);
    if (windowHandle && windowHandle->screen()) {
        return windowHandle->screen();
    }

    return QGuiApplication::primaryScreen();
}

Monitor::Monitor(QWidget* parent)
    : ScreenPreviewWidget(parent)
{
    QRect avail = screenFromWidget(this)->geometry();
    setRatio((qreal)avail.width() / (qreal)avail.height());
    for (int i = 0;
            i < 8;
            ++i)
        popups[ i ] = new QMenu(this);
    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene, this);
    view->setBackgroundBrush(Qt::black);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setFocusPolicy(Qt::NoFocus);
    view->setFrameShape(QFrame::NoFrame);
    for (int i = 0;
            i < 8;
            ++i) {
        items[ i ] = new Corner(this);
        scene->addItem(items[ i ]);
        hidden[ i ] = false;
        grp[ i ] = new QActionGroup(this);
    }
    checkSize();
}

void Monitor::clear()
{
    for (int i = 0;
            i < 8;
            ++i) {
        popups[ i ]->clear();
        setEdge(i, false);
        setEdgeHidden(i, false);
        delete grp[ i ];
        grp[ i ] = new QActionGroup(this);
    }
}

void Monitor::resizeEvent(QResizeEvent* e)
{
    ScreenPreviewWidget::resizeEvent(e);
    checkSize();
}

void Monitor::checkSize()
{
    QRect contentsRect = previewRect();
    //int w = 151;
    //int h = 115;
    view->setGeometry(contentsRect);
    scene->setSceneRect(QRect(QPoint(0, 0), contentsRect.size()));
    int x2 = (contentsRect.width() - 20) / 2;
    int x3 = contentsRect.width() - 20;
    int y2 = (contentsRect.height() - 20) / 2;
    int y3 = contentsRect.height() - 20;
    items[ 0 ]->setRect(0, y2, 20, 20);
    items[ 1 ]->setRect(x3, y2, 20, 20);
    items[ 2 ]->setRect(x2, 0, 20, 20);
    items[ 3 ]->setRect(x2, y3, 20, 20);
    items[ 4 ]->setRect(0, 0, 20, 20);
    items[ 5 ]->setRect(x3, 0, 20, 20);
    items[ 6 ]->setRect(0, y3, 20, 20);
    items[ 7 ]->setRect(x3, y3, 20, 20);
}

void Monitor::setEdge(int edge, bool set)
{
    items[ edge ]->setActive(set);
}

bool Monitor::edge(int edge) const
{
    return items[ edge ]->brush() == Qt::green;
}

void Monitor::setEdgeEnabled(int edge, bool enabled)
{
    for (QAction *action : qAsConst(popup_actions[edge])) {
        action->setEnabled(enabled);
    }
}

void Monitor::setEdgeHidden(int edge, bool set)
{
    hidden[ edge ] = set;
    if (set)
        items[ edge ]->hide();
    else
        items[ edge ]->show();
}

bool Monitor::edgeHidden(int edge) const
{
    return hidden[ edge ];
}

void Monitor::addEdgeItem(int edge, const QString& item)
{
    QAction* act = popups[ edge ]->addAction(item);
    act->setCheckable(true);
    popup_actions[ edge ].append(act);
    grp[ edge ]->addAction(act);
    if (popup_actions[ edge ].count() == 1) {
        act->setChecked(true);
        items[ edge ]->setToolTip(item);
    }
    setEdge(edge, !popup_actions[ edge ][ 0 ]->isChecked());
}

void Monitor::setEdgeItemEnabled(int edge, int index, bool enabled)
{
    popup_actions[ edge ][ index ]->setEnabled(enabled);
}

bool Monitor::edgeItemEnabled(int edge, int index) const
{
    return popup_actions[ edge ][ index ]->isEnabled();
}

void Monitor::selectEdgeItem(int edge, int index)
{
    popup_actions[ edge ][ index ]->setChecked(true);
    setEdge(edge, !popup_actions[ edge ][ 0 ]->isChecked());
    QString actionText = popup_actions[ edge ][ index ]->text();
    // remove accelerators added by KAcceleratorManager
    actionText = KLocalizedString::removeAcceleratorMarker(actionText);
    items[ edge ]->setToolTip(actionText);
}

int Monitor::selectedEdgeItem(int edge) const
{
    foreach (QAction * act, popup_actions[ edge ])
    if (act->isChecked())
        return popup_actions[ edge ].indexOf(act);
    abort();
}

void Monitor::popup(Corner* c, QPoint pos)
{
    for (int i = 0;
            i < 8;
            ++i) {
        if (items[ i ] == c) {
            if (popup_actions[ i ].count() == 0)
                return;
            if (QAction* a = popups[ i ]->exec(pos)) {
                selectEdgeItem(i, popup_actions[ i ].indexOf(a));
                emit changed();
                emit edgeSelectionChanged(i, popup_actions[ i ].indexOf(a));
                c->setToolTip(KLocalizedString::removeAcceleratorMarker(a->text()));
            }
            return;
        }
    }
    abort();
}

void Monitor::flip(Corner* c, QPoint pos)
{
    for (int i = 0;
            i < 8;
            ++i) {
        if (items[ i ] == c) {
            if (popup_actions[ i ].count() == 0)
                setEdge(i, !edge(i));
            else
                popup(c, pos);
            return;
        }
    }
    abort();
}

Monitor::Corner::Corner(Monitor* m)
    : monitor(m),
      m_active(false),
      m_hover(false)
{
    button = new Plasma::FrameSvg();
    button->setImagePath("widgets/button");
    setAcceptHoverEvents(true);
}

Monitor::Corner::~Corner()
{
    delete button;
}

void Monitor::Corner::contextMenuEvent(QGraphicsSceneContextMenuEvent* e)
{
    monitor->popup(this, e->screenPos());
}

void Monitor::Corner::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
    monitor->flip(this, e->screenPos());
}

void Monitor::Corner::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (m_hover) {
        button->setElementPrefix("normal");

        qreal left, top, right, bottom;
        button->getMargins(left, top, right, bottom);

        button->setElementPrefix("active");
        qreal activeLeft, activeTop, activeRight, activeBottom;
        button->getMargins(activeLeft, activeTop, activeRight, activeBottom);

        QRectF activeRect = QRectF(QPointF(0, 0), rect().size());
        activeRect.adjust(left - activeLeft, top - activeTop,
                          -(right - activeRight), -(bottom - activeBottom));
        button->setElementPrefix("active");
        button->resizeFrame(activeRect.size());
        button->paintFrame(painter, rect().topLeft() + activeRect.topLeft());
    } else {
        button->setElementPrefix(m_active ? "pressed" : "normal");
        button->resizeFrame(rect().size());
        button->paintFrame(painter, rect().topLeft());
    }

    if (m_active) {
        QPainterPath roundedRect;
        painter->setRenderHint(QPainter::Antialiasing);
        roundedRect.addRoundedRect(rect().adjusted(5, 5, -5, -5), 2, 2);
        painter->fillPath(roundedRect, QApplication::palette().text());
    }
}

void Monitor::Corner::hoverEnterEvent(QGraphicsSceneHoverEvent * e)
{
    Q_UNUSED(e);
    m_hover = true;
    update();
}

void Monitor::Corner::hoverLeaveEvent(QGraphicsSceneHoverEvent * e)
{
    Q_UNUSED(e);
    m_hover = false;
    update();
}

void Monitor::Corner::setActive(bool active)
{
    m_active = active;
    update();
}

bool Monitor::Corner::active() const
{
    return m_active;
}
} // namespace

