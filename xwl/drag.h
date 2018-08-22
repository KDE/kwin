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
#ifndef KWIN_XWL_DRAG
#define KWIN_XWL_DRAG

#include <KWayland/Client/datadevicemanager.h>

#include <QPoint>

#include <xcb/xcb.h>

namespace KWin
{
class Toplevel;

namespace Xwl
{
enum class DragEventReply;

using DnDAction = KWayland::Client::DataDeviceManager::DnDAction;

/**
 * An ongoing drag operation.
 */
class Drag : public QObject
{
    Q_OBJECT
public:
    static void sendClientMessage(xcb_window_t target, xcb_atom_t type, xcb_client_message_data_t *data);
    static DnDAction atomToClientAction(xcb_atom_t atom);
    static xcb_atom_t clientActionToAtom(DnDAction action);

    virtual ~Drag() = default;
    virtual bool handleClientMessage(xcb_client_message_event_t *event) = 0;
    virtual DragEventReply moveFilter(Toplevel *target, QPoint pos) = 0;

    virtual bool end() = 0;

Q_SIGNALS:
    void finish(Drag *self);
};

}
}

#endif
