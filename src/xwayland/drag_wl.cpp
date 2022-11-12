/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drag_wl.h"

#include "databridge.h"
#include "dnd.h"
#include "xwayland.h"
#include "xwldrophandler.h"

#include "atoms.h"
#include "wayland/datadevice_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <QMouseEvent>
#include <QTimer>

using DnDAction = KWaylandServer::DataDeviceManagerInterface::DnDAction;
using DnDActions = KWaylandServer::DataDeviceManagerInterface::DnDActions;

namespace KWin
{
namespace Xwl
{

WlToXDrag::WlToXDrag(Dnd *dnd)
    : m_dnd(dnd)
{
}

DragEventReply WlToXDrag::moveFilter(Window *target, const QPoint &pos)
{
    return DragEventReply::Wayland;
}

bool WlToXDrag::handleClientMessage(xcb_client_message_event_t *event)
{
    return m_dnd->dropHandler()->handleClientMessage(event);
}

Xvisit::Xvisit(Window *target, KWaylandServer::AbstractDataSource *dataSource, Dnd *dnd, QObject *parent)
    : QObject(parent)
    , m_dnd(dnd)
    , m_target(target)
    , m_dataSource(dataSource)
{
    // first check supported DND version
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_get_property_cookie_t cookie = xcb_get_property(xcbConn,
                                                        0,
                                                        m_target->window(),
                                                        atoms->xdnd_aware,
                                                        XCB_GET_PROPERTY_TYPE_ANY,
                                                        0, 1);
    auto *reply = xcb_get_property_reply(xcbConn, cookie, nullptr);
    if (!reply) {
        doFinish();
        return;
    }
    if (reply->type != XCB_ATOM_ATOM) {
        doFinish();
        free(reply);
        return;
    }
    xcb_atom_t *value = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
    m_version = std::min(*value, Dnd::version());
    if (m_version < 1) {
        // minimal version we accept is 1
        doFinish();
        free(reply);
        return;
    }
    free(reply);

    receiveOffer();
}

bool Xvisit::handleClientMessage(xcb_client_message_event_t *event)
{
    if (event->type == atoms->xdnd_status) {
        return handleStatus(event);
    } else if (event->type == atoms->xdnd_finished) {
        return handleFinished(event);
    }
    return false;
}

bool Xvisit::handleStatus(xcb_client_message_event_t *event)
{
    xcb_client_message_data_t *data = &event->data;
    if (data->data32[0] != m_target->window()) {
        // wrong target window
        return false;
    }

    m_accepts = data->data32[1] & 1;
    xcb_atom_t actionAtom = data->data32[4];

    if (m_dataSource && !m_dataSource->mimeTypes().isEmpty()) {
        m_dataSource->accept(m_accepts ? m_dataSource->mimeTypes().constFirst() : QString());
    }
    // TODO: we could optimize via rectangle in data32[2] and data32[3]

    // position round trip finished
    m_pos.pending = false;

    if (!m_state.dropped) {
        // as long as the drop is not yet done determine requested action
        m_preferredAction = Dnd::atomToClientAction(actionAtom);
        determineProposedAction();
        requestDragAndDropAction();
    }

    if (m_pos.cached) {
        // send cached position
        m_pos.cached = false;
        sendPosition(m_pos.cache);
    } else if (m_state.dropped) {
        // drop was done in between, now close it out
        drop();
    }
    return true;
}

bool Xvisit::handleFinished(xcb_client_message_event_t *event)
{
    xcb_client_message_data_t *data = &event->data;

    if (data->data32[0] != m_target->window()) {
        // different target window
        return false;
    }

    if (!m_state.dropped) {
        // drop was never done
        doFinish();
        return true;
    }

    if (m_dataSource) {
        m_dataSource->dndFinished();
    }
    doFinish();
    return true;
}

void Xvisit::sendPosition(const QPointF &globalPos)
{
    const int16_t x = globalPos.x();
    const int16_t y = globalPos.y();

    if (m_pos.pending) {
        m_pos.cache = QPoint(x, y);
        m_pos.cached = true;
        return;
    }
    m_pos.pending = true;

    xcb_client_message_data_t data = {};
    data.data32[0] = m_dnd->window();
    data.data32[2] = (x << 16) | y;
    data.data32[3] = XCB_CURRENT_TIME;
    data.data32[4] = Dnd::clientActionToAtom(m_proposedAction);

    Drag::sendClientMessage(m_target->window(), atoms->xdnd_position, &data);
}

void Xvisit::leave()
{
    if (m_state.dropped) {
        // dropped, but not yet finished, it'll be cleaned up when the drag finishes
        return;
    }
    if (m_state.finished) {
        // was already finished
        return;
    }
    // we only need to leave, when we entered before
    if (m_state.entered) {
        sendLeave();
    }
    doFinish();
}

void Xvisit::receiveOffer()
{
    retrieveSupportedActions();
    connect(m_dataSource, &KWaylandServer::AbstractDataSource::supportedDragAndDropActionsChanged,
            this, &Xvisit::retrieveSupportedActions);
    enter();
}

void Xvisit::enter()
{
    m_state.entered = true;
    // send enter event and current position to X client
    sendEnter();
    sendPosition(waylandServer()->seat()->pointerPos());

    // proxy future pointer position changes
    m_motionConnection = connect(waylandServer()->seat(),
                                 &KWaylandServer::SeatInterface::pointerPosChanged,
                                 this, &Xvisit::sendPosition);
}

void Xvisit::sendEnter()
{
    if (!m_dataSource) {
        return;
    }

    xcb_client_message_data_t data = {};
    data.data32[0] = m_dnd->window();
    data.data32[1] = m_version << 24;

    const auto mimeTypesNames = m_dataSource->mimeTypes();
    const int mimesCount = mimeTypesNames.size();
    // Number of written entries in data32
    size_t cnt = 2;
    // Number of mimeTypes
    size_t totalCnt = 0;
    for (const auto &mimeName : mimeTypesNames) {
        // 3 mimes and less can be sent directly in the XdndEnter message
        if (totalCnt == 3) {
            break;
        }
        const auto atom = Selection::mimeTypeToAtom(mimeName);

        if (atom != XCB_ATOM_NONE) {
            data.data32[cnt] = atom;
            cnt++;
        }
        totalCnt++;
    }
    for (int i = cnt; i < 5; i++) {
        data.data32[i] = XCB_ATOM_NONE;
    }

    if (mimesCount > 3) {
        // need to first transfer all available mime types
        data.data32[1] |= 1;

        QVector<xcb_atom_t> targets;
        targets.resize(mimesCount);

        size_t cnt = 0;
        for (const auto &mimeName : mimeTypesNames) {
            const auto atom = Selection::mimeTypeToAtom(mimeName);
            if (atom != XCB_ATOM_NONE) {
                targets[cnt] = atom;
                cnt++;
            }
        }

        xcb_change_property(kwinApp()->x11Connection(),
                            XCB_PROP_MODE_REPLACE,
                            m_dnd->window(),
                            atoms->xdnd_type_list,
                            XCB_ATOM_ATOM,
                            32, cnt, targets.data());
    }
    Drag::sendClientMessage(m_target->window(), atoms->xdnd_enter, &data);
}

void Xvisit::sendDrop(uint32_t time)
{
    xcb_client_message_data_t data = {};
    data.data32[0] = m_dnd->window();
    data.data32[2] = time;

    Drag::sendClientMessage(m_target->window(), atoms->xdnd_drop, &data);

    if (m_version < 2) {
        doFinish();
    }
}

void Xvisit::sendLeave()
{
    xcb_client_message_data_t data = {};
    data.data32[0] = m_dnd->window();
    Drag::sendClientMessage(m_target->window(), atoms->xdnd_leave, &data);
}

void Xvisit::retrieveSupportedActions()
{
    m_supportedActions = m_dataSource->supportedDragAndDropActions();
    determineProposedAction();
    requestDragAndDropAction();
}

void Xvisit::determineProposedAction()
{
    DnDAction oldProposedAction = m_proposedAction;
    if (m_supportedActions.testFlag(m_preferredAction)) {
        m_proposedAction = m_preferredAction;
    } else if (m_supportedActions.testFlag(DnDAction::Copy)) {
        m_proposedAction = DnDAction::Copy;
    } else {
        m_proposedAction = DnDAction::None;
    }
    // send updated action to X target
    if (oldProposedAction != m_proposedAction && m_state.entered) {
        sendPosition(waylandServer()->seat()->pointerPos());
    }
}

void Xvisit::requestDragAndDropAction()
{
    DnDAction action = m_preferredAction != DnDAction::None ? m_preferredAction : DnDAction::Copy;
    // we assume the X client supports Move, but this might be wrong - then
    // the drag just cancels, if the user tries to force it.

    // As we skip the client data device, we do action negotiation directly then tell the source.
    if (m_supportedActions.testFlag(action)) {
        // everything is supported, no changes are needed
    } else if (m_supportedActions.testFlag(DnDAction::Copy)) {
        action = DnDAction::Copy;
    } else if (m_supportedActions.testFlag(DnDAction::Move)) {
        action = DnDAction::Move;
    }
    if (m_dataSource) {
        m_dataSource->dndAction(action);
    }
}

void Xvisit::drop()
{
    Q_ASSERT(!m_state.finished);
    m_state.dropped = true;
    // stop further updates
    // TODO: revisit when we allow ask action
    stopConnections();
    if (!m_state.entered) {
        // wait for enter (init + offers)
        return;
    }
    if (m_pos.pending) {
        // wait for pending position roundtrip
        return;
    }
    if (!m_accepts) {
        // target does not accept current action/offer
        sendLeave();
        doFinish();
        return;
    }
    // dnd session ended successfully
    sendDrop(XCB_CURRENT_TIME);
}

void Xvisit::doFinish()
{
    m_state.finished = true;
    m_pos.cached = false;
    stopConnections();
    Q_EMIT finish(this);
}

void Xvisit::stopConnections()
{
    // final outcome has been determined from Wayland side
    // no more updates needed
    disconnect(m_motionConnection);
    m_motionConnection = QMetaObject::Connection();
}

} // namespace Xwl
} // namespace KWin
