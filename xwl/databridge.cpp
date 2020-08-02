/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*********************************************************************/
#include "databridge.h"
#include "clipboard.h"
#include "dnd.h"
#include "selection.h"
#include "xcbutils.h"
#include "xwayland.h"

#include "abstract_client.h"
#include "atoms.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/seat.h>

#include <KWaylandServer/datadevicemanager_interface.h>
#include <KWaylandServer/datadevice_interface.h>
#include <KWaylandServer/seat_interface.h>

using namespace KWayland::Client;
using namespace KWaylandServer;

namespace KWin
{
namespace Xwl
{

static DataBridge *s_self = nullptr;

DataBridge *DataBridge::self()
{
    return s_self;
}

DataBridge::DataBridge(QObject *parent)
    : QObject(parent)
{
    s_self = this;

    DataDeviceManager *dataDeviceManager = waylandServer()->internalDataDeviceManager();
    Seat *seat = waylandServer()->internalSeat();
    m_dataDevice = dataDeviceManager->getDataDevice(seat, this);

    const DataDeviceManagerInterface *dataDeviceManagerInterface =
        waylandServer()->dataDeviceManager();

    auto *dc = new QMetaObject::Connection();
    *dc = connect(dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated, this,
        [this, dc](DataDeviceInterface *dataDeviceInterface) {
            if (m_dataDeviceInterface) {
                return;
            }
            if (dataDeviceInterface->client() != waylandServer()->internalConnection()) {
                return;
            }
            QObject::disconnect(*dc);
            delete dc;
            m_dataDeviceInterface = dataDeviceInterface;
            init();
        }
    );

    waylandServer()->dispatch();
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
    if (m_clipboard && m_clipboard->filterEvent(event)) {
        return true;
    }
    if (m_dnd && m_dnd->filterEvent(event)) {
        return true;
    }
    if (event->response_type == Xcb::Extensions::self()->fixesSelectionNotifyEvent()) {
        return handleXfixesNotify((xcb_xfixes_selection_notify_event_t *)event);
    }
    return false;
}

bool DataBridge::handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    Selection *selection = nullptr;

    if (event->selection == atoms->clipboard) {
        selection = m_clipboard;
    } else if (event->selection == atoms->xdnd_selection) {
        selection = m_dnd;
    }

    if (!selection) {
        return false;
    }

    return selection->handleXfixesNotify(event);
}

DragEventReply DataBridge::dragMoveFilter(Toplevel *target, const QPoint &pos)
{
    if (!m_dnd) {
        return DragEventReply::Wayland;
    }
    return m_dnd->dragMoveFilter(target, pos);
}

} // namespace Xwl
} // namespace KWin
