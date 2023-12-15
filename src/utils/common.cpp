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
#include <kkeyserver.h>
#endif

#include <QPainter>
#include <QWidget>

#ifndef KCMRULES
#include <QApplication>
#include <QDebug>
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

static bool keyboard_grabbed = false;

bool grabXKeyboard(xcb_window_t w)
{
    if (QWidget::keyboardGrabber() != nullptr) {
        return false;
    }
    if (keyboard_grabbed) {
        qCDebug(KWIN_CORE) << "Failed to grab X Keyboard: already grabbed by us";
        return false;
    }
    if (qApp->activePopupWidget() != nullptr) {
        qCDebug(KWIN_CORE) << "Failed to grab X Keyboard: no popup widget";
        return false;
    }
    if (w == XCB_WINDOW_NONE) {
        w = rootWindow();
    }
    const xcb_grab_keyboard_cookie_t c = xcb_grab_keyboard_unchecked(connection(), false, w, xTime(),
                                                                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    UniqueCPtr<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(connection(), c, nullptr));
    if (!grab) {
        qCDebug(KWIN_CORE) << "Failed to grab X Keyboard: grab null";
        return false;
    }
    if (grab->status != XCB_GRAB_STATUS_SUCCESS) {
        qCDebug(KWIN_CORE) << "Failed to grab X Keyboard: grab failed with status" << grab->status;
        return false;
    }
    keyboard_grabbed = true;
    return true;
}

void ungrabXKeyboard()
{
    if (!keyboard_grabbed) {
        // grabXKeyboard() may fail sometimes, so don't fail, but at least warn anyway
        qCDebug(KWIN_CORE) << "ungrabXKeyboard() called but keyboard not grabbed!";
    }
    keyboard_grabbed = false;
    xcb_ungrab_keyboard(connection(), XCB_TIME_CURRENT_TIME);
}

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states

Qt::MouseButton x11ToQtMouseButton(int button)
{
    if (button == XCB_BUTTON_INDEX_1) {
        return Qt::LeftButton;
    }
    if (button == XCB_BUTTON_INDEX_2) {
        return Qt::MiddleButton;
    }
    if (button == XCB_BUTTON_INDEX_3) {
        return Qt::RightButton;
    }
    if (button == XCB_BUTTON_INDEX_4) {
        return Qt::XButton1;
    }
    if (button == XCB_BUTTON_INDEX_5) {
        return Qt::XButton2;
    }
    return Qt::NoButton;
}

Qt::MouseButtons x11ToQtMouseButtons(int state)
{
    Qt::MouseButtons ret = {};
    if (state & XCB_KEY_BUT_MASK_BUTTON_1) {
        ret |= Qt::LeftButton;
    }
    if (state & XCB_KEY_BUT_MASK_BUTTON_2) {
        ret |= Qt::MiddleButton;
    }
    if (state & XCB_KEY_BUT_MASK_BUTTON_3) {
        ret |= Qt::RightButton;
    }
    if (state & XCB_KEY_BUT_MASK_BUTTON_4) {
        ret |= Qt::XButton1;
    }
    if (state & XCB_KEY_BUT_MASK_BUTTON_5) {
        ret |= Qt::XButton2;
    }
    return ret;
}

Qt::KeyboardModifiers x11ToQtKeyboardModifiers(int state)
{
    Qt::KeyboardModifiers ret = {};
    if (state & XCB_KEY_BUT_MASK_SHIFT) {
        ret |= Qt::ShiftModifier;
    }
    if (state & XCB_KEY_BUT_MASK_CONTROL) {
        ret |= Qt::ControlModifier;
    }
    if (state & KKeyServer::modXAlt()) {
        ret |= Qt::AltModifier;
    }
    if (state & KKeyServer::modXMeta()) {
        ret |= Qt::MetaModifier;
    }
    return ret;
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
