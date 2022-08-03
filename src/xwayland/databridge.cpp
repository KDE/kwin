/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "databridge.h"
#include "clipboard.h"
#include "dnd.h"
#include "primary.h"
#include "selection.h"
#include "xwayland.h"

#include "atoms.h"
#include "wayland/clientconnection.h"
#include "wayland/datadevice_interface.h"
#include "wayland/datadevicemanager_interface.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

using namespace KWaylandServer;

namespace KWin
{
namespace Xwl
{

DataBridge::DataBridge()
{
    init();
}

void DataBridge::init()
{
    m_clipboard = new Clipboard(atoms->clipboard, this);
    m_dnd = new Dnd(atoms->xdnd_selection, this);
    m_primary = new Primary(atoms->primary, this);
    kwinApp()->installNativeEventFilter(this);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool DataBridge::nativeEventFilter(const QByteArray &eventType, void *message, long int *)
#else
bool DataBridge::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
#endif
{
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
        return m_clipboard->filterEvent(event) || m_dnd->filterEvent(event) || m_primary->filterEvent(event);
    }
    return false;
}

DragEventReply DataBridge::dragMoveFilter(Window *target, const QPoint &pos)
{
    return m_dnd->dragMoveFilter(target, pos);
}

} // namespace Xwl
} // namespace KWin
