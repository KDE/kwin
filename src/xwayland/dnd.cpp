/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dnd.h"

#include "databridge.h"
#include "drag_wl.h"
#include "drag_x.h"
#include "selection_source.h"

#include "atoms.h"
#include "wayland/compositor.h"
#include "wayland/datasource.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xwayland.h"
#include "xwldrophandler.h"

#include <QMouseEvent>

#include <xcb/xcb.h>

namespace KWin
{
namespace Xwl
{

// version of DnD support in X
const static uint32_t s_version = 5;
uint32_t Dnd::version()
{
    return s_version;
}

XwlDropHandler *Dnd::dropHandler() const
{
    return m_dropHandler;
}

Dnd::Dnd(xcb_atom_t atom, QObject *parent)
    : Selection(atom, parent)
    , m_dropHandler(new XwlDropHandler(this))
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    const uint32_t dndValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      window(),
                      kwinApp()->x11RootWindow(),
                      0, 0,
                      8192, 8192, // TODO: get current screen size and connect to changes
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      dndValues);
    registerXfixes();

    xcb_change_property(xcbConn,
                        XCB_PROP_MODE_REPLACE,
                        window(),
                        atoms->xdnd_aware,
                        XCB_ATOM_ATOM,
                        32, 1, &s_version);
    xcb_flush(xcbConn);

    connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, &Dnd::startDrag);
    connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, &Dnd::endDrag);
}

void Dnd::doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    if (qobject_cast<XToWlDrag *>(m_currentDrag)) {
        // X drag is in progress, rogue X client took over the selection.
        return;
    }
    if (m_currentDrag) {
        // Wl drag is in progress - don't overwrite by rogue X client,
        // get it back instead!
        ownSelection(true);
        return;
    }
    createX11Source(nullptr);
    const auto *seat = waylandServer()->seat();
    auto *originSurface = seat->focusedPointerSurface();
    if (!originSurface) {
        return;
    }
    if (originSurface->client() != waylandServer()->xWaylandConnection()) {
        // focused surface client is not Xwayland - do not allow drag to start
        // TODO: can we make this stronger (window id comparison)?
        return;
    }
    if (!seat->isPointerButtonPressed(Qt::LeftButton)) {
        // we only allow drags to be started on (left) pointer button being
        // pressed for now
        return;
    }
    createX11Source(event);
    if (X11Source *source = x11Source()) {
        SeatInterface *seat = waylandServer()->seat();
        seat->startDrag(source->dataSource(), seat->focusedPointerSurface(), seat->pointerButtonSerial(Qt::LeftButton));
    }
}

void Dnd::x11OfferLost()
{
}

void Dnd::x11OffersChanged(const QStringList &added, const QStringList &removed)
{
}

bool Dnd::handleClientMessage(xcb_client_message_event_t *event)
{
    for (Drag *drag : std::as_const(m_oldDrags)) {
        if (drag->handleClientMessage(event)) {
            return true;
        }
    }
    if (m_currentDrag && m_currentDrag->handleClientMessage(event)) {
        return true;
    }
    return false;
}

DragEventReply Dnd::dragMoveFilter(Window *target)
{
    Q_ASSERT(m_currentDrag);
    return m_currentDrag->moveFilter(target);
}

void Dnd::startDrag()
{
    // There can only ever be one Wl native drag at the same time.
    Q_ASSERT(!m_currentDrag);

    auto dragSource = waylandServer()->seat()->dragSource();
    if (qobject_cast<XwlDataSource *>(dragSource)) {
        m_currentDrag = new XToWlDrag(x11Source(), this);
    } else {
        m_currentDrag = new WlToXDrag(this);
        auto source = new WlSource(this);
        source->setDataSourceIface(dragSource);
        connect(dragSource, &AbstractDataSource::aboutToBeDestroyed, this, [this, source] {
            if (source == wlSource()) {
                setWlSource(nullptr);
            }
        });
        setWlSource(source);
        ownSelection(true);
    }
}

void Dnd::endDrag()
{
    Q_ASSERT(m_currentDrag);

    connect(m_currentDrag, &Drag::finish, this, &Dnd::clearOldDrag);
    m_oldDrags << m_currentDrag;

    m_currentDrag = nullptr;
}

void Dnd::clearOldDrag(Drag *drag)
{
    m_oldDrags.removeOne(drag);
    delete drag;
}

using DnDAction = DataDeviceManagerInterface::DnDAction;
using DnDActions = DataDeviceManagerInterface::DnDActions;

DnDAction Dnd::atomToClientAction(xcb_atom_t atom)
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

xcb_atom_t Dnd::clientActionToAtom(DnDAction action)
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

#include "moc_dnd.cpp"
