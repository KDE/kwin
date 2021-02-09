/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drag.h"

#include "atoms.h"

namespace KWin
{
namespace Xwl
{

Drag::Drag(QObject *parent)
    : QObject(parent)
{
}

Drag::~Drag()
{
}

void Drag::sendClientMessage(xcb_window_t target, xcb_atom_t type, xcb_client_message_data_t *data)
{
    xcb_client_message_event_t event {
        XCB_CLIENT_MESSAGE, // response_type
        32,         // format
        0,          // sequence
        target,     // window
        type,       // type
        *data,      // data
    };

    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_send_event(xcbConn,
                   0,
                   target,
                   XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char *>(&event));
    xcb_flush(xcbConn);
}

DnDAction Drag::atomToClientAction(xcb_atom_t atom)
{
    if (atom == atoms->xdnd_action_copy) {
        return DnDAction::Copy;
    } else if (atom == atoms->xdnd_action_move) {
        return DnDAction::Move;
    } else if (atom == atoms->xdnd_action_ask) {
        // we currently do not support it - need some test client first
        return DnDAction::None;
//        return DnDAction::Ask;
    }
    return DnDAction::None;
}

xcb_atom_t Drag::clientActionToAtom(DnDAction action)
{
    if (action == DnDAction::Copy) {
        return atoms->xdnd_action_copy;
    } else if (action == DnDAction::Move) {
        return atoms->xdnd_action_move;
    } else if (action == DnDAction::Ask) {
        // we currently do not support it - need some test client first
        return XCB_ATOM_NONE;
//        return atoms->xdnd_action_ask;
    }
    return XCB_ATOM_NONE;
}

} // namespace Xwl
} // namespace KWin
