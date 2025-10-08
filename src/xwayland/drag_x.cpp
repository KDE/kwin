/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drag_x.h"

#include "databridge.h"
#include "datasource.h"
#include "dnd.h"
#include "selection_source.h"
#include "xwayland.h"

#include "atoms.h"
#include "wayland/datadevice.h"
#include "wayland/datasource.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <QMouseEvent>
#include <QTimer>

namespace KWin
{
namespace Xwl
{

using DnDAction = KWin::DataDeviceManagerInterface::DnDAction;
using DnDActions = KWin::DataDeviceManagerInterface::DnDActions;

XToWlDrag::XToWlDrag(X11Source *source, Dnd *dnd)
    : m_dnd(dnd)
    , m_source(source)
{
    connect(source->dataSource(), &XwlDataSource::dropped, this, [this] {
        if (m_visit) {
            connect(m_visit, &WlVisit::finish, this, [this](WlVisit *visit) {
                tryFinish();
            });

            QTimer::singleShot(2000, this, [this] {
                if (m_visit->isEntered() && m_visit->isDropHandled()) {
                    m_visit->sendFinished();
                }
                Q_EMIT finish(this);
            });
        }
    });
    connect(source->dataSource(), &XwlDataSource::finished, this, [this] {
        tryFinish();
    });
    connect(source->dataSource(), &XwlDataSource::cancelled, this, [this] {
        if (m_visit && !m_visit->leave()) {
            connect(m_visit, &WlVisit::finish, this, &XToWlDrag::tryFinish);
        }
        tryFinish();
    });
    connect(source->dataSource(), &XwlDataSource::dataRequested, source, &X11Source::transferRequested);
}

XToWlDrag::~XToWlDrag()
{
}

bool XToWlDrag::moveFilter(Window *target, const QPointF &position)
{
    auto *seat = waylandServer()->seat();
    if (!seat->isDragPointer()) {
        return false;
    }

    seat->notifyPointerMotion(position);
    seat->notifyDragMotion(position);

    if (m_visit && m_visit->target() == target) {
        // still same Wl target, wait for X events
        return true;
    }
    if (m_visit) {
        if (m_visit->leave()) {
            delete m_visit;
        } else {
            connect(m_visit, &WlVisit::finish, this, [this](WlVisit *visit) {
                m_oldVisits.removeOne(visit);
                delete visit;
            });
            m_oldVisits << m_visit;
        }
    }
    const bool hasCurrent = m_visit;
    m_visit = nullptr;

    if (!target || !target->surface() || target->surface()->client() == waylandServer()->xWaylandConnection()) {
        // currently there is no target or target is an Xwayland window
        // handled here and by X directly
        if (hasCurrent) {
            // last received enter event is now void,
            // wait for the next one
            seat->setDragTarget(nullptr, nullptr, QPointF(), QMatrix4x4());
        }
        return true;
    }
    // new Wl native target
    auto *ac = static_cast<Window *>(target);
    m_visit = new WlVisit(ac, this, m_dnd);
    connect(m_visit, &WlVisit::entered, this, &XToWlDrag::setMimeTypes);
    return true;
}

bool XToWlDrag::handleClientMessage(xcb_client_message_event_t *event)
{
    for (auto *visit : std::as_const(m_oldVisits)) {
        if (visit->handleClientMessage(event)) {
            return true;
        }
    }
    if (m_visit && m_visit->handleClientMessage(event)) {
        return true;
    }
    return false;
}

void XToWlDrag::setDragAndDropAction(DnDAction action)
{
    m_source->dataSource()->setSupportedDndActions(action);
}

DnDAction XToWlDrag::selectedDragAndDropAction()
{
    return m_source->dataSource()->selectedDndAction();
}

void XToWlDrag::setMimeTypes(const QStringList &mimeTypes)
{
    m_source->dataSource()->setMimeTypes(mimeTypes);
    setDragTarget();
}

void XToWlDrag::setDragTarget()
{
    if (!m_visit) {
        return;
    }

    auto *ac = m_visit->target();

    auto seat = waylandServer()->seat();
    auto dropTarget = seat->dropHandlerForSurface(ac->surface());

    if (!dropTarget || !ac->surface()) {
        return;
    }
    seat->setDragTarget(dropTarget, ac->surface(), seat->pointerPos(), ac->inputTransformation());
}

void XToWlDrag::tryFinish()
{
    if (!m_visit) {
        // not dropped above Wl native target
        Q_EMIT finish(this);
        return;
    }

    // Avoid sending XdndFinish if neither XdndDrop nor XdndLeave event has been received yet.
    if (!m_visit->isFinished()) {
        return;
    }

    // Avoid sending XdndFinish if wl_data_offer.finish has not been called yet.
    if (!m_source->dataSource()->isDndCancelled() && !m_source->dataSource()->isDndFinished()) {
        return;
    }

    m_visit->sendFinished();
    Q_EMIT finish(this);
}

WlVisit::WlVisit(Window *target, XToWlDrag *drag, Dnd *dnd)
    : QObject(drag)
    , m_dnd(dnd)
    , m_target(target)
    , m_drag(drag)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    m_window = xcb_generate_id(xcbConn);
    m_dnd->overwriteRequestorWindow(m_window);

    const uint32_t dndValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
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

    uint32_t version = Dnd::version();
    xcb_change_property(xcbConn,
                        XCB_PROP_MODE_REPLACE,
                        m_window,
                        atoms->xdnd_aware,
                        XCB_ATOM_ATOM,
                        32, 1, &version);

    xcb_map_window(xcbConn, m_window);
    workspace()->addManualOverlay(m_window);
    workspace()->updateStackingOrder(true);

    xcb_flush(xcbConn);
    m_mapped = true;
}

WlVisit::~WlVisit()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_destroy_window(xcbConn, m_window);
    xcb_flush(xcbConn);
}

bool WlVisit::leave()
{
    m_dnd->overwriteRequestorWindow(XCB_WINDOW_NONE);
    unmapProxyWindow();
    return m_finished;
}

bool WlVisit::handleClientMessage(xcb_client_message_event_t *event)
{
    if (event->window != m_window) {
        // different window
        return false;
    }

    if (event->type == atoms->xdnd_enter) {
        return handleEnter(event);
    } else if (event->type == atoms->xdnd_position) {
        return handlePosition(event);
    } else if (event->type == atoms->xdnd_drop) {
        return handleDrop(event);
    } else if (event->type == atoms->xdnd_leave) {
        return handleLeave(event);
    }
    return false;
}

static QList<xcb_atom_t> mimeTypeListFromWindow(xcb_window_t window)
{
    const uint32_t length = 0x1fffffff;
    xcb_get_property_cookie_t cookie = xcb_get_property(kwinApp()->x11Connection(), 0, window, atoms->xdnd_type_list, XCB_ATOM_ATOM, 0, length);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(kwinApp()->x11Connection(), cookie, nullptr);
    if (!reply) {
        return QList<xcb_atom_t>();
    }

    const xcb_atom_t *mimeAtoms = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
    const QList<xcb_atom_t> atoms(mimeAtoms, mimeAtoms + reply->value_len);
    free(reply);

    return atoms;
}

bool WlVisit::handleEnter(xcb_client_message_event_t *event)
{
    if (m_entered) {
        // a drag already entered
        return true;
    }
    m_entered = true;

    xcb_client_message_data_t *data = &event->data;
    m_srcWindow = data->data32[0];
    m_version = data->data32[1] >> 24;

    // Bit 0 in data32[1] indicates whether mime types are stored in the XdndTypeList property or in data32[2..5].
    QStringList mimeTypes;
    if (data->data32[1] & 1) {
        const auto atoms = mimeTypeListFromWindow(m_srcWindow);
        for (const xcb_atom_t &atom : atoms) {
            mimeTypes += Xcb::atomToMimeTypes(atom);
        }
    } else {
        for (int i = 2; i < 5; i++) {
            mimeTypes += Xcb::atomToMimeTypes(data->data32[i]);
        }
    }

    Q_EMIT entered(mimeTypes);
    return true;
}

bool WlVisit::handlePosition(xcb_client_message_event_t *event)
{
    xcb_client_message_data_t *data = &event->data;
    m_srcWindow = data->data32[0];

    if (!m_target) {
        // not over Wl window at the moment
        m_action = DnDAction::None;
        m_actionAtom = XCB_ATOM_NONE;
        sendStatus();
        return true;
    }

    const xcb_timestamp_t timestamp = data->data32[3];
    m_drag->x11Source()->setTimestamp(timestamp);

    xcb_atom_t actionAtom = m_version > 1 ? data->data32[4] : atoms->xdnd_action_copy;
    auto action = Dnd::atomToClientAction(actionAtom);

    if (action == DnDAction::None) {
        // copy action is always possible in XDND
        action = DnDAction::Copy;
        actionAtom = atoms->xdnd_action_copy;
    }

    if (m_action != action) {
        m_action = action;
        m_actionAtom = actionAtom;
        m_drag->setDragAndDropAction(m_action);
    }

    sendStatus();
    return true;
}

bool WlVisit::handleDrop(xcb_client_message_event_t *event)
{
    m_dropHandled = true;

    xcb_client_message_data_t *data = &event->data;
    m_srcWindow = data->data32[0];
    const xcb_timestamp_t timestamp = data->data32[2];
    m_drag->x11Source()->setTimestamp(timestamp);

    // we do nothing more here, the drop is being processed
    // through the X11Source object
    doFinish();
    return true;
}

void WlVisit::doFinish()
{
    m_finished = true;
    unmapProxyWindow();
    Q_EMIT finish(this);
}

bool WlVisit::handleLeave(xcb_client_message_event_t *event)
{
    m_entered = false;
    xcb_client_message_data_t *data = &event->data;
    m_srcWindow = data->data32[0];
    doFinish();
    return true;
}

void WlVisit::sendStatus()
{
    // receive position events
    uint32_t flags = 1 << 1;
    if (targetAcceptsAction()) {
        // accept the drop
        flags |= (1 << 0);
    }
    xcb_client_message_data_t data = {};
    data.data32[0] = m_window;
    data.data32[1] = flags;
    data.data32[4] = flags & (1 << 0) ? m_actionAtom : static_cast<uint32_t>(XCB_ATOM_NONE);
    Drag::sendClientMessage(m_srcWindow, atoms->xdnd_status, &data);
}

void WlVisit::sendFinished()
{
    const bool accepted = m_entered && m_action != DnDAction::None;
    xcb_client_message_data_t data = {};
    data.data32[0] = m_window;
    data.data32[1] = accepted;
    data.data32[2] = accepted ? m_actionAtom : static_cast<uint32_t>(XCB_ATOM_NONE);
    Drag::sendClientMessage(m_srcWindow, atoms->xdnd_finished, &data);
}

bool WlVisit::targetAcceptsAction() const
{
    if (m_action == DnDAction::None) {
        return false;
    }
    const auto selAction = m_drag->selectedDragAndDropAction();
    return selAction == m_action || selAction == DnDAction::Copy;
}

void WlVisit::unmapProxyWindow()
{
    if (!m_mapped) {
        return;
    }
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_unmap_window(xcbConn, m_window);
    workspace()->removeManualOverlay(m_window);
    workspace()->updateStackingOrder(true);
    xcb_flush(xcbConn);
    m_mapped = false;
}

} // namespace Xwl
} // namespace KWin

#include "moc_drag_x.cpp"
