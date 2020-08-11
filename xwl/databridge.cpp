/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "databridge.h"
#include "clipboard.h"
#include "dnd.h"
#include "selection.h"
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

KWIN_SINGLETON_FACTORY(DataBridge)

void DataBridge::destroy()
{
    delete s_self;
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
    kwinApp()->installNativeEventFilter(this);
}

bool DataBridge::nativeEventFilter(const QByteArray &eventType, void *message, long int *)
{
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
        return m_clipboard->filterEvent(event) || m_dnd->filterEvent(event);
    }
    return false;
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
