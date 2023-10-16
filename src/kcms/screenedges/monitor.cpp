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
#include <QGraphicsScene>
#include <QGraphicsSceneEvent>
#include <QGraphicsView>
#include <QMenu>
#include <QScreen>
#include <QWindow>

namespace KWin
{

static QScreen *screenFromWidget(const QWidget *widget)
{
    QScreen *screen = widget->screen();
    if (screen) {
        return screen;
    }

    return QGuiApplication::primaryScreen();
}

Monitor::Monitor(QWidget *parent)
    : ScreenPreviewWidget(parent)
{
    for (auto &popup : m_popups) {
        popup = std::make_unique<QMenu>(this);
    }
    m_scene = std::make_unique<QGraphicsScene>(this);
    m_view = std::make_unique<QGraphicsView>(m_scene.get(), this);
    m_view->setBackgroundBrush(Qt::black);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setFocusPolicy(Qt::NoFocus);
    m_view->setFrameShape(QFrame::NoFrame);
    for (size_t i = 0; i < m_items.size(); i++) {
        m_items[i] = std::make_unique<Corner>(this);
        m_scene->addItem(m_items[i].get());
        m_hidden[i] = false;
        m_actionGroups[i] = std::make_unique<QActionGroup>(this);
    }
    QRect avail = screenFromWidget(this)->geometry();
    setMinimumContentWidth(20 * 3 + 5 * 2); // 3 buttons in a row and some spacing between them
    setRatio((qreal)avail.width() / (qreal)avail.height());
    checkSize();
}

Monitor::~Monitor() = default;

void Monitor::clear()
{
    for (size_t i = 0; i < m_popups.size(); i++) {
        m_popups[i]->clear();
        m_items[i]->setActive(false);
        setEdgeHidden(i, false);
        m_actionGroups[i] = std::make_unique<QActionGroup>(this);
    }
}

void Monitor::resizeEvent(QResizeEvent *e)
{
    ScreenPreviewWidget::resizeEvent(e);
    checkSize();
}

bool Monitor::event(QEvent *event)
{
    const bool r = ScreenPreviewWidget::event(event);
    if (event->type() == QEvent::ScreenChangeInternal) {
        QRect avail = screenFromWidget(this)->geometry();
        setRatio((qreal)avail.width() / (qreal)avail.height());
        checkSize();
    }
    return r;
}

void Monitor::checkSize()
{
    QRect contentsRect = previewRect();
    m_view->setGeometry(contentsRect);
    m_scene->setSceneRect(QRect(QPoint(0, 0), contentsRect.size()));
    const int x2 = (contentsRect.width() - 20) / 2;
    const int x3 = contentsRect.width() - 20;
    const int y2 = (contentsRect.height() - 20) / 2;
    const int y3 = contentsRect.height() - 20;
    m_items[0]->setRect(0, y2, 20, 20);
    m_items[1]->setRect(x3, y2, 20, 20);
    m_items[2]->setRect(x2, 0, 20, 20);
    m_items[3]->setRect(x2, y3, 20, 20);
    m_items[4]->setRect(0, 0, 20, 20);
    m_items[5]->setRect(x3, 0, 20, 20);
    m_items[6]->setRect(0, y3, 20, 20);
    m_items[7]->setRect(x3, y3, 20, 20);
}

void Monitor::setEdgeEnabled(int edge, bool enabled)
{
    for (QAction *action : std::as_const(m_popupActions[edge])) {
        action->setEnabled(enabled);
    }
}

void Monitor::setEdgeHidden(int edge, bool set)
{
    m_hidden[edge] = set;
    if (set) {
        m_items[edge]->hide();
    } else {
        m_items[edge]->show();
    }
}

bool Monitor::edgeHidden(int edge) const
{
    return m_hidden[edge];
}

void Monitor::addEdgeItem(int edge, const QString &item)
{
    QAction *act = m_popups[edge]->addAction(item);
    act->setCheckable(true);
    m_popupActions[edge].append(act);
    m_actionGroups[edge]->addAction(act);
    if (m_popupActions[edge].count() == 1) {
        act->setChecked(true);
        m_items[edge]->setToolTip(item);
    }
    m_items[edge]->setActive(!m_popupActions[edge].front()->isChecked());
}

void Monitor::setEdgeItemEnabled(int edge, int index, bool enabled)
{
    m_popupActions[edge][index]->setEnabled(enabled);
}

bool Monitor::edgeItemEnabled(int edge, int index) const
{
    return m_popupActions[edge][index]->isEnabled();
}

void Monitor::selectEdgeItem(int edge, int index)
{
    m_popupActions[edge][index]->setChecked(true);
    m_items[edge]->setActive(!m_popupActions[edge].front()->isChecked());
    QString actionText = m_popupActions[edge][index]->text();
    // remove accelerators added by KAcceleratorManager
    actionText = KLocalizedString::removeAcceleratorMarker(actionText);
    m_items[edge]->setToolTip(actionText);
}

int Monitor::selectedEdgeItem(int edge) const
{
    const auto &actions = m_popupActions[edge];
    for (QAction *act : actions) {
        if (act->isChecked()) {
            return actions.indexOf(act);
        }
    }
    Q_UNREACHABLE();
}

void Monitor::popup(Corner *c, QPoint pos)
{
    for (size_t i = 0; i < m_items.size(); i++) {
        if (m_items[i].get() == c) {
            if (m_popupActions[i].empty()) {
                return;
            }
            if (QAction *a = m_popups[i]->exec(pos)) {
                selectEdgeItem(i, m_popupActions[i].indexOf(a));
                Q_EMIT changed();
                Q_EMIT edgeSelectionChanged(i, m_popupActions[i].indexOf(a));
                c->setToolTip(KLocalizedString::removeAcceleratorMarker(a->text()));
            }
            return;
        }
    }
    Q_UNREACHABLE();
}

void Monitor::flip(Corner *c, QPoint pos)
{
    for (size_t i = 0; i < m_items.size(); i++) {
        if (m_items[i].get() == c) {
            if (m_popupActions[i].empty()) {
                m_items[i]->setActive(m_items[i]->brush() != Qt::green);
            } else {
                popup(c, pos);
            }
            return;
        }
    }
    Q_UNREACHABLE();
}

Monitor::Corner::Corner(Monitor *m)
    : m_monitor(m)
    , m_button(std::make_unique<Plasma::FrameSvg>())
{
    m_button->setImagePath("widgets/button");
    setAcceptHoverEvents(true);
}

Monitor::Corner::~Corner() = default;

void Monitor::Corner::contextMenuEvent(QGraphicsSceneContextMenuEvent *e)
{
    m_monitor->popup(this, e->screenPos());
}

void Monitor::Corner::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    m_monitor->flip(this, e->screenPos());
}

void Monitor::Corner::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_hover) {
        m_button->setElementPrefix("normal");

        qreal left, top, right, bottom;
        m_button->getMargins(left, top, right, bottom);

        m_button->setElementPrefix("active");
        qreal activeLeft, activeTop, activeRight, activeBottom;
        m_button->getMargins(activeLeft, activeTop, activeRight, activeBottom);

        QRectF activeRect = QRectF(QPointF(0, 0), rect().size());
        activeRect.adjust(left - activeLeft, top - activeTop,
                          -(right - activeRight), -(bottom - activeBottom));
        m_button->setElementPrefix("active");
        m_button->resizeFrame(activeRect.size());
        m_button->paintFrame(painter, rect().topLeft() + activeRect.topLeft());
    } else {
        m_button->setElementPrefix(m_active ? "pressed" : "normal");
        m_button->resizeFrame(rect().size());
        m_button->paintFrame(painter, rect().topLeft());
    }

    if (m_active) {
        QPainterPath roundedRect;
        painter->setRenderHint(QPainter::Antialiasing);
        roundedRect.addRoundedRect(rect().adjusted(5, 5, -5, -5), 2, 2);
        painter->fillPath(roundedRect, QApplication::palette().text());
    }
}

void Monitor::Corner::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    m_hover = true;
    update();
}

void Monitor::Corner::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
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
