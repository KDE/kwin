/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_filter.h"

#include "effects.h"
#include "screenedge.h"
#include "tabbox/tabbox.h"
#include "workspace.h"

#include <KKeyServer>

#include <xcb/xcb.h>

namespace KWin
{
namespace TabBox
{

X11Filter::X11Filter()
    : X11EventFilter(QVector<int>{XCB_KEY_PRESS, XCB_KEY_RELEASE, XCB_MOTION_NOTIFY, XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE})
{
}

bool X11Filter::event(xcb_generic_event_t *event)
{
    const auto tab = workspace()->tabbox();
    if (!tab->isGrabbed()) {
        return false;
    }
    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE: {
        auto e = reinterpret_cast<xcb_button_press_event_t *>(event);
        xcb_allow_events(connection(), XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
        if (!tab->isShown() && tab->isDisplayed()) {
            if (effects && static_cast<EffectsHandlerImpl *>(effects)->isMouseInterception()) {
                // pass on to effects, effects will filter out the event
                return false;
            }
        }
        if (eventType == XCB_BUTTON_PRESS) {
            return buttonPress(e);
        }
        return false;
    }
    case XCB_MOTION_NOTIFY: {
        motion(event);
        break;
    }
    case XCB_KEY_PRESS: {
        keyPress(event);
        return true;
    }
    case XCB_KEY_RELEASE:
        keyRelease(event);
        return true;
    }
    return false;
}
bool X11Filter::buttonPress(xcb_button_press_event_t *event)
{
    // press outside Tabbox?
    const auto tab = workspace()->tabbox();
    QPoint pos(event->root_x, event->root_y);
    if ((!tab->isShown() && tab->isDisplayed())
        || (!tabBox->containsPos(pos) && (event->detail == XCB_BUTTON_INDEX_1 || event->detail == XCB_BUTTON_INDEX_2 || event->detail == XCB_BUTTON_INDEX_3))) {
        tab->close(); // click outside closes tab
        return true;
    }
    if (event->detail == XCB_BUTTON_INDEX_5 || event->detail == XCB_BUTTON_INDEX_4) {
        // mouse wheel event
        const QModelIndex index = tabBox->nextPrev(event->detail == XCB_BUTTON_INDEX_5);
        if (index.isValid()) {
            tab->setCurrentIndex(index);
        }
        return true;
    }
    return false;
}

void X11Filter::motion(xcb_generic_event_t *event)
{
    auto *mouseEvent = reinterpret_cast<xcb_motion_notify_event_t *>(event);
    const QPoint rootPos(mouseEvent->root_x, mouseEvent->root_y);
    // TODO: this should be in ScreenEdges directly
    workspace()->screenEdges()->check(rootPos, QDateTime::fromMSecsSinceEpoch(xTime(), Qt::UTC), true);
    xcb_allow_events(connection(), XCB_ALLOW_ASYNC_POINTER, XCB_CURRENT_TIME);
}

void X11Filter::keyPress(xcb_generic_event_t *event)
{
    int keyQt;
    xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
    KKeyServer::xcbKeyPressEventToQt(keyEvent, &keyQt);
    workspace()->tabbox()->keyPress(keyQt);
}

void X11Filter::keyRelease(xcb_generic_event_t *event)
{
    const auto ev = reinterpret_cast<xcb_key_release_event_t *>(event);
    unsigned int mk = ev->state & (KKeyServer::modXShift() | KKeyServer::modXCtrl() | KKeyServer::modXAlt() | KKeyServer::modXMeta());
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for (int i = XCB_MAP_INDEX_SHIFT;
         i <= XCB_MAP_INDEX_5;
         ++i) {
        if ((mk & (1 << i)) != 0) {
            if (mod_index >= 0) {
                return;
            }
            mod_index = i;
        }
    }
    bool release = false;
    if (mod_index == -1) {
        release = true;
    } else {
        Xcb::ModifierMapping xmk;
        if (xmk) {
            xcb_keycode_t *keycodes = xmk.keycodes();
            const int maxIndex = xmk.size();
            for (int i = 0; i < xmk->keycodes_per_modifier; ++i) {
                const int index = xmk->keycodes_per_modifier * mod_index + i;
                if (index >= maxIndex) {
                    continue;
                }
                if (keycodes[index] == ev->detail) {
                    release = true;
                }
            }
        }
    }
    if (release) {
        workspace()->tabbox()->modifiersReleased();
    }
}

}
}
