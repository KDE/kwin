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

KWIN_SINGLETON_FACTORY(DataBridge)

void DataBridge::destroy()
{
    delete s_self;
}

DataBridge::DataBridge(QObject *parent)
    : QObject(parent)
{
    s_self = this;
    init();
}

DataBridge::~DataBridge()
{
    s_self = nullptr;
}

void DataBridge::init()
{
    m_clipboard = new Clipboard(atoms->clipboard, this);
    m_dnd = new Dnd(atoms->xdnd_selection, this);
    m_primary = new Primary(atoms->primary, this);
}


DragEventReply DataBridge::dragMoveFilter(Window *target, const QPoint &pos)
{
    return m_dnd->dragMoveFilter(target, pos);
}

} // namespace Xwl
} // namespace KWin
