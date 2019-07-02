/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
