/*
    SPDX-FileCopyrightText: 2017 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/
#include "popup_input_filter.h"
#include "deleted.h"
#include "input_event.h"
#include "internalwindow.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

PopupInputFilter::PopupInputFilter()
    : QObject()
{
    connect(workspace(), &Workspace::windowAdded, this, &PopupInputFilter::handleWindowAdded);
    connect(workspace(), &Workspace::internalWindowAdded, this, &PopupInputFilter::handleWindowAdded);
}

void PopupInputFilter::handleWindowAdded(Window *window)
{
    if (m_popupWindows.contains(window)) {
        return;
    }
    if (window->hasPopupGrab()) {
        // TODO: verify that the Window is allowed as a popup
        connect(window, &Window::windowShown, this, &PopupInputFilter::handleWindowAdded, Qt::UniqueConnection);
        connect(window, &Window::windowClosed, this, &PopupInputFilter::handleWindowRemoved, Qt::UniqueConnection);
        m_popupWindows << window;
    }
}

void PopupInputFilter::handleWindowRemoved(Window *window)
{
    m_popupWindows.removeOne(window);
}

bool PopupInputFilter::pointerEvent(MouseEvent *event, quint32 nativeButton)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }
    if (event->type() == QMouseEvent::MouseButtonPress) {
        auto pointerFocus = input()->findToplevel(event->globalPos());
        if (!pointerFocus || !Window::belongToSameApplication(pointerFocus, m_popupWindows.constLast())) {
            // a press on a window (or no window) not belonging to the popup window
            cancelPopups();
            // filter out this press
            return true;
        }
        if (pointerFocus && pointerFocus->isDecorated()) {
            // test whether it is on the decoration
            if (!pointerFocus->clientGeometry().contains(event->globalPos())) {
                cancelPopups();
                return true;
            }
        }
    }
    return false;
}

bool PopupInputFilter::keyEvent(KeyEvent *event)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }

    auto seat = waylandServer()->seat();

    auto last = m_popupWindows.last();
    if (last->surface() == nullptr) {
        return false;
    }

    seat->setFocusedKeyboardSurface(last->surface());

    if (!passToInputMethod(event)) {
        passToWaylandServer(event);
    }

    return true;
}

bool PopupInputFilter::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }
    auto pointerFocus = input()->findToplevel(pos);
    if (!pointerFocus || !Window::belongToSameApplication(pointerFocus, m_popupWindows.constLast())) {
        // a touch on a window (or no window) not belonging to the popup window
        cancelPopups();
        // filter out this touch
        return true;
    }
    if (pointerFocus && pointerFocus->isDecorated()) {
        // test whether it is on the decoration
        if (!pointerFocus->clientGeometry().contains(pos)) {
            cancelPopups();
            return true;
        }
    }
    return false;
}

void PopupInputFilter::cancelPopups()
{
    while (!m_popupWindows.isEmpty()) {
        auto c = m_popupWindows.takeLast();
        c->popupDone();
    }
}

}
