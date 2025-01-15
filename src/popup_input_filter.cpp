/*
    SPDX-FileCopyrightText: 2017 Martin Graesslin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/
#include "popup_input_filter.h"
#include "input_event.h"
#include "internalwindow.h"
#include "keyboard_input.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "waylandwindow.h"
#include "window.h"
#include "workspace.h"

#include <qpa/qwindowsysteminterface.h>

namespace KWin
{

PopupInputFilter::PopupInputFilter()
    : QObject()
    , InputEventFilter(InputFilterOrder::Popup)
{
    connect(workspace(), &Workspace::windowAdded, this, &PopupInputFilter::handleWindowAdded);
}

void PopupInputFilter::handleWindowAdded(Window *window)
{
    if (m_popupWindows.contains(window)) {
        return;
    }
    if (window->hasPopupGrab()) {
        // TODO: verify that the Window is allowed as a popup
        m_popupWindows << window;
        focus(window);

        connect(window, &Window::closed, this, [this, window]() {
            m_popupWindows.removeOne(window);
            // Move focus to the parent popup. If that's the last popup, then move focus back to the parent
            if (!m_popupWindows.isEmpty()) {
                focus(m_popupWindows.constLast());
            } else {
                input()->keyboard()->update();
            }
        });
    }
}

bool PopupInputFilter::pointerButton(PointerButtonEvent *event)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }
    if (event->state == PointerButtonState::Pressed) {
        auto pointerFocus = input()->findToplevel(event->position);
        if (!pointerFocus || !Window::belongToSameApplication(pointerFocus, m_popupWindows.constLast())) {
            // a press on a window (or no window) not belonging to the popup window
            cancelPopups();
            // filter out this press
            return true;
        }
        if (pointerFocus && pointerFocus->isDecorated()) {
            // test whether it is on the decoration
            if (!exclusiveContains(pointerFocus->clientGeometry(), event->position)) {
                cancelPopups();
                return true;
            }
        }
    }
    return false;
}

bool PopupInputFilter::keyboardKey(KeyboardKeyEvent *event)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }

    Window *last = m_popupWindows.last();
    focus(last);

    if (auto internalWindow = qobject_cast<InternalWindow *>(last)) {
        QWindowSystemInterface::handleExtendedKeyEvent(internalWindow->handle(),
                                                       event->state != KeyboardKeyState::Released ? QEvent::KeyPress : QEvent::KeyRelease,
                                                       event->key,
                                                       event->modifiers,
                                                       event->nativeScanCode,
                                                       event->nativeVirtualKey,
                                                       0,
                                                       event->text,
                                                       event->state == KeyboardKeyState::Repeated);
    } else if (qobject_cast<WaylandWindow *>(last)) {
        if (!passToInputMethod(event)) {
            if (event->state == KeyboardKeyState::Repeated) {
                return true;
            }

            waylandServer()->seat()->setTimestamp(event->timestamp);
            waylandServer()->seat()->notifyKeyboardKey(event->nativeScanCode, event->state);
        }
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
        if (!exclusiveContains(pointerFocus->clientGeometry(), pos)) {
            cancelPopups();
            return true;
        }
    }
    return false;
}

bool PopupInputFilter::tabletToolTipEvent(TabletToolTipEvent *event)
{
    if (m_popupWindows.isEmpty()) {
        return false;
    }
    if (event->type == TabletToolTipEvent::Type::Press) {
        auto tabletFocus = input()->findToplevel(event->position);
        if (!tabletFocus || !Window::belongToSameApplication(tabletFocus, m_popupWindows.constLast())) {
            // a touch on a window (or no window) not belonging to the popup window
            cancelPopups();
            // filter out this touch
            return true;
        }
        if (tabletFocus && tabletFocus->isDecorated()) {
            // test whether it is on the decoration
            if (!exclusiveContains(tabletFocus->clientGeometry(), event->position)) {
                cancelPopups();
                return true;
            }
        }
    }
    return false;
}

void PopupInputFilter::focus(Window *popup)
{
    if (auto internalWindow = qobject_cast<InternalWindow *>(m_popupWindows.constLast())) {
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        if (QGuiApplication::focusWindow() != internalWindow->handle()) {
            QWindowSystemInterface::handleFocusWindowChanged(internalWindow->handle());
        }
    } else if (auto waylandWindow = qobject_cast<WaylandWindow *>(m_popupWindows.constLast())) {
        if (QGuiApplication::focusWindow()) {
            QWindowSystemInterface::handleFocusWindowChanged(nullptr);
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(waylandWindow->surface(), input()->keyboard()->unfilteredKeys());
    }
}

void PopupInputFilter::cancelPopups()
{
    while (!m_popupWindows.isEmpty()) {
        auto c = m_popupWindows.takeLast();
        c->popupDone();
    }
}

}

#include "moc_popup_input_filter.cpp"
