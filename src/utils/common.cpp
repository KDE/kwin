/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils/common.h"
#include "utils/c_ptr.h"

#if KWIN_BUILD_X11
#include "effect/xcb.h"
#endif

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_OPENGL, "kwin_scene_opengl", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_QPAINTER, "kwin_scene_qpainter", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD, "kwin_virtualkeyboard", QtWarningMsg)
namespace KWin
{

#ifndef KCMRULES

//************************************
// StrutRect
//************************************

StrutRect::StrutRect(QRect rect, StrutArea area)
    : QRect(rect)
    , m_area(area)
{
}

StrutRect::StrutRect(int x, int y, int width, int height, StrutArea area)
    : QRect(x, y, width, height)
    , m_area(area)
{
}

StrutRect::StrutRect(const StrutRect &other)
    : QRect(other)
    , m_area(other.area())
{
}

StrutRect &StrutRect::operator=(const StrutRect &other)
{
    if (this != &other) {
        QRect::operator=(other);
        m_area = other.area();
    }
    return *this;
}

#if KWIN_BUILD_X11

static int server_grab_count = 0;

void grabXServer()
{
    if (++server_grab_count == 1) {
        xcb_grab_server(connection());
    }
}

void ungrabXServer()
{
    Q_ASSERT(server_grab_count > 0);
    if (--server_grab_count == 0) {
        xcb_ungrab_server(connection());
        xcb_flush(connection());
    }
}

#endif
#endif

QPointF popupOffset(const QRectF &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity, const QSizeF popupSize)
{
    QPointF anchorPoint;
    switch (anchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        anchorPoint.setX(anchorRect.x());
        break;
    case Qt::RightEdge:
        anchorPoint.setX(anchorRect.x() + anchorRect.width());
        break;
    default:
        anchorPoint.setX(qRound(anchorRect.x() + anchorRect.width() / 2.0));
    }
    switch (anchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        anchorPoint.setY(anchorRect.y());
        break;
    case Qt::BottomEdge:
        anchorPoint.setY(anchorRect.y() + anchorRect.height());
        break;
    default:
        anchorPoint.setY(qRound(anchorRect.y() + anchorRect.height() / 2.0));
    }

    // calculate where the top left point of the popup will end up with the applied gravity
    // gravity indicates direction. i.e if gravitating towards the top the popup's bottom edge
    // will next to the anchor point
    QPointF popupPosAdjust;
    switch (gravity & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        popupPosAdjust.setX(-popupSize.width());
        break;
    case Qt::RightEdge:
        popupPosAdjust.setX(0);
        break;
    default:
        popupPosAdjust.setX(qRound(-popupSize.width() / 2.0));
    }
    switch (gravity & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        popupPosAdjust.setY(-popupSize.height());
        break;
    case Qt::BottomEdge:
        popupPosAdjust.setY(0);
        break;
    default:
        popupPosAdjust.setY(qRound(-popupSize.height() / 2.0));
    }

    return anchorPoint + popupPosAdjust;
}

QRectF gravitateGeometry(const QRectF &rect, const QRectF &bounds, Gravity gravity)
{
    QRectF geometry = rect;

    switch (gravity) {
    case Gravity::TopLeft:
        geometry.moveRight(bounds.right());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Top:
    case Gravity::TopRight:
        geometry.moveLeft(bounds.left());
        geometry.moveBottom(bounds.bottom());
        break;
    case Gravity::Right:
    case Gravity::BottomRight:
    case Gravity::Bottom:
    case Gravity::None:
        geometry.moveLeft(bounds.left());
        geometry.moveTop(bounds.top());
        break;
    case Gravity::BottomLeft:
    case Gravity::Left:
        geometry.moveRight(bounds.right());
        geometry.moveTop(bounds.top());
        break;
    }

    return geometry;
}

} // namespace
