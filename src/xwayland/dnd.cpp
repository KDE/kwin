/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dnd.h"
#include "atoms.h"
#include "databridge.h"
#include "drag_wl.h"
#include "drag_x.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include "xwayland.h"
#include "xwldrophandler.h"

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
    m_window = xcb_generate_id(kwinApp()->x11Connection());
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      m_window,
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

    connect(waylandServer()->seat(), &SeatInterface::dragStarted, this, &Dnd::startDrag);
    connect(waylandServer()->seat(), &SeatInterface::dragEnded, this, &Dnd::endDrag);
}

Dnd::~Dnd()
{
    // If there is current dnd originating from an x client, we do not want to process the dragEnded()
    // signal while the Dnd is already partially destroyed.
    disconnect(waylandServer()->seat(), &SeatInterface::dragStarted, this, &Dnd::startDrag);
    disconnect(waylandServer()->seat(), &SeatInterface::dragEnded, this, &Dnd::endDrag);
    m_xSource.reset();
}

void Dnd::selectionDisowned()
{
}

void Dnd::selectionClaimed(xcb_xfixes_selection_notify_event_t *event)
{
    if (m_currentDrag) {
        return;
    }

    m_xSource.reset();

    SeatInterface *seat = waylandServer()->seat();
    auto *originSurface = seat->focusedPointerSurface();
    if (!originSurface) {
        return;
    }
    if (originSurface->client() != waylandServer()->xWaylandConnection()) {
        return;
    }
    if (!seat->isPointerButtonPressed(Qt::LeftButton)) {
        // we only allow drags to be started on (left) pointer button being
        // pressed for now
        return;
    }

    m_xSource = std::make_unique<XwlDataSource>(this);
    seat->startPointerDrag(m_xSource.get(), seat->focusedPointerSurface(), seat->pointerPos(), seat->focusedPointerSurfaceTransformation(), seat->pointerButtonSerial(Qt::LeftButton));
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

bool Dnd::dragMoveFilter(Window *target, const QPointF &position)
{
    Q_ASSERT(m_currentDrag);
    return m_currentDrag->moveFilter(target, position);
}

void Dnd::startDrag()
{
    Q_ASSERT(!m_currentDrag);

    auto dragSource = waylandServer()->seat()->dragSource();
    if (ownsDataSource(dragSource)) {
        m_currentDrag = new XToWlDrag(x11Source(), this);
        setWlSource(nullptr);
    } else {
        m_currentDrag = new WlToXDrag(this);
        connect(dragSource, &AbstractDataSource::aboutToBeDestroyed, this, [this, dragSource] {
            if (dragSource == wlSource()) {
                setWlSource(nullptr);
            }
        });
        setWlSource(dragSource);
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
