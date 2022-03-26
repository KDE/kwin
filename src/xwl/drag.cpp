/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
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
    xcb_client_message_event_t event{
        XCB_CLIENT_MESSAGE, // response_type
        32, // format
        0, // sequence
        target, // window
        type, // type
        *data, // data
    };
    static_assert(sizeof(event) == 32, "Would leak stack data otherwise");

    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_send_event(xcbConn,
                   0,
                   target,
                   XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char *>(&event));
    xcb_flush(xcbConn);
}

} // namespace Xwl
} // namespace KWin
