/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

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
#include "databridge.h"
#include "xwayland.h"
#include "selection.h"
#include "clipboard.h"
#include "dnd.h"

#include "atoms.h"
#include "wayland_server.h"
#include "workspace.h"
#include "abstract_client.h"

// KWayland
#include <KWayland/Client/seat.h>
#include <KWayland/Client/datadevicemanager.h>

#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/datadevicemanager_interface.h>
#include <KWayland/Server/datadevice_interface.h>

namespace KWin {
namespace Xwl {

static DataBridge *s_self = nullptr;
DataBridge* DataBridge::self()
{
    return s_self;
}

DataBridge::DataBridge(QObject *parent)
    : QObject(parent)
{
    s_self = this;
    auto *ddm = waylandServer()->internalDataDeviceManager();
    auto *seat = waylandServer()->internalSeat();
    m_dd = ddm->getDataDevice(seat, this);
    waylandServer()->dispatch();

    const auto *ddmi = waylandServer()->dataDeviceManager();
    auto *dc = new QMetaObject::Connection();
    *dc = connect(ddmi, &KWayland::Server::DataDeviceManagerInterface::dataDeviceCreated, this,
                 [this, dc](KWayland::Server::DataDeviceInterface *ddi) {
                    if (m_ddi || ddi->client() != waylandServer()->internalConnection()) {
                        return;
                    }
                    QObject::disconnect(*dc);
                    delete dc;
                    m_ddi = ddi;
                    init();
                }
    );
}

DataBridge::~DataBridge()
{
    s_self = nullptr;
}

void DataBridge::init()
{
    m_clipboard = new Clipboard(atoms->clipboard, this);
    m_dnd = new Dnd(atoms->xdnd_selection, this);
    waylandServer()->dispatch();
}

bool DataBridge::filterEvent(xcb_generic_event_t *event)
{
    if (m_clipboard->filterEvent(event)) {
        return true;
    }
    if (m_dnd->filterEvent(event)) {
        return true;
    }
    if (event->response_type - Xwayland::self()->xfixes()->first_event == XCB_XFIXES_SELECTION_NOTIFY) {
        return handleXfixesNotify((xcb_xfixes_selection_notify_event_t *)event);
    }
    return false;
}

bool DataBridge::handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    auto getSelection = [this](xcb_atom_t atom) -> Selection* {
        if (atom == atoms->clipboard) {
            return m_clipboard;
        }
        if (atom == atoms->xdnd_selection) {
            return m_dnd;
        }
        return nullptr;
    };
    auto *sel = getSelection(event->selection);
    return sel && sel->handleXfixesNotify(event);
}

DragEventReply DataBridge::dragMoveFilter(Toplevel *target, QPoint pos)
{
    if (!m_dnd) {
        return DragEventReply::Wayland;
    }
    return m_dnd->dragMoveFilter(target, pos);
}

}
}
