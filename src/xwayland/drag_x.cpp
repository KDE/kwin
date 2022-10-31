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
#include "wayland/datadevice_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <QMouseEvent>
#include <QTimer>

namespace KWin
{
namespace Xwl
{

using DnDAction = KWaylandServer::DataDeviceManagerInterface::DnDAction;
using DnDActions = KWaylandServer::DataDeviceManagerInterface::DnDActions;

static QStringList atomToMimeTypes(xcb_atom_t atom)
{
    QStringList mimeTypes;

    if (atom == atoms->utf8_string) {
        mimeTypes << QString::fromLatin1("text/plain;charset=utf-8");
    } else if (atom == atoms->text) {
        mimeTypes << QString::fromLatin1("text/plain");
    } else if (atom == atoms->uri_list) {
        mimeTypes << QString::fromLatin1("text/uri-list") << QString::fromLatin1("text/x-uri");
    } else if (atom == atoms->moz_url) {
        mimeTypes << QStringLiteral("text/x-moz-url");
    } else if (atom == atoms->netscape_url) {
        mimeTypes << QStringLiteral("_NETSCAPE_URL");
    } else {
        mimeTypes << Selection::atomName(atom);
    }
    return mimeTypes;
}

XToWlDrag::XToWlDrag(X11Source *source, Dnd *dnd)
    : m_dnd(dnd)
    , m_source(source)
{
    connect(m_dnd, &Dnd::transferFinished, this, [this](xcb_timestamp_t eventTime) {
        // we use this mechanism, because the finished call is not
        // reliable done by Wayland clients
        auto it = std::find_if(m_dataRequests.begin(), m_dataRequests.end(), [eventTime](const QPair<xcb_timestamp_t, bool> &req) {
            return req.first == eventTime && req.second == false;
        });
        if (it == m_dataRequests.end()) {
            // transfer finished for a different drag
            return;
        }
        (*it).second = true;
        checkForFinished();
    });
    connect(source, &X11Source::transferReady, this, [this](xcb_atom_t target, qint32 fd) {
        m_dataRequests << QPair<xcb_timestamp_t, bool>(m_source->timestamp(), false);
    });
    connect(&m_selectionSource, &XwlDataSource::dropped, this, [this] {
        m_performed = true;
        if (m_visit) {
            connect(m_visit, &WlVisit::finish, this, [this](WlVisit *visit) {
                checkForFinished();
            });

            QTimer::singleShot(2000, this, [this] {
                if (!m_visit->entered() || !m_visit->dropHandled()) {
                    // X client timed out
                    Q_EMIT finish(this);
                } else if (m_dataRequests.size() == 0) {
                    // Wl client timed out
                    m_visit->sendFinished();
                    Q_EMIT finish(this);
                }
            });
        }
        // Dave do we need this async finish check anymore?
        checkForFinished();
    });
    connect(&m_selectionSource, &XwlDataSource::finished, this, [this] {
        checkForFinished();
    });
    connect(&m_selectionSource, &XwlDataSource::cancelled, this, [this] {
        if (m_visit && !m_visit->leave()) {
            connect(m_visit, &WlVisit::finish, this, &XToWlDrag::checkForFinished);
        }
        checkForFinished();
    });
    connect(&m_selectionSource, &XwlDataSource::dataRequested, source, &X11Source::startTransfer);

    auto *seat = waylandServer()->seat();
    int serial = waylandServer()->seat()->pointerButtonSerial(Qt::LeftButton);
    // we know we are the focussed surface as dnd checks
    seat->startDrag(&m_selectionSource, seat->focusedPointerSurface(), serial);
}

XToWlDrag::~XToWlDrag()
{
}

DragEventReply XToWlDrag::moveFilter(Window *target, const QPoint &pos)
{
    auto *seat = waylandServer()->seat();

    if (m_visit && m_visit->target() == target) {
        // still same Wl target, wait for X events
        return DragEventReply::Ignore;
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
            seat->setDragTarget(nullptr, nullptr);
        }
        return DragEventReply::Ignore;
    }
    // new Wl native target
    auto *ac = static_cast<Window *>(target);
    m_visit = new WlVisit(ac, this, m_dnd);
    connect(m_visit, &WlVisit::offersReceived, this, &XToWlDrag::setOffers);
    return DragEventReply::Ignore;
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
    m_selectionSource.setSupportedDndActions(action);
}

DnDAction XToWlDrag::selectedDragAndDropAction()
{
    return m_selectionSource.selectedDndAction();
}

void XToWlDrag::setOffers(const Mimes &offers)
{
    m_source->setOffers(offers);
    if (offers.isEmpty()) {
        // There are no offers, so just directly set the drag target,
        // no transfer possible anyways.
        setDragTarget();
        return;
    }
    if (m_offers == offers) {
        // offers had been set already by a previous visit
        // Wl side is already configured
        setDragTarget();
        return;
    }

    m_offers = offers;
    QStringList mimeTypes;
    mimeTypes.reserve(offers.size());
    for (const auto &mimePair : offers) {
        mimeTypes.append(mimePair.first);
    }
    m_selectionSource.setMimeTypes(mimeTypes);
    setDragTarget();
}

using Mime = QPair<QString, xcb_atom_t>;

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
    seat->setDragTarget(dropTarget, ac->surface(), ac->inputTransformation());
}

bool XToWlDrag::checkForFinished()
{
    if (!m_visit) {
        // not dropped above Wl native target
        Q_EMIT finish(this);
        return true;
    }
    if (!m_visit->finished()) {
        return false;
    }
    if (m_dataRequests.size() == 0 && m_selectionSource.isAccepted()) {
        // need to wait for first data request
        return false;
    }
    const bool transfersFinished = std::all_of(m_dataRequests.begin(), m_dataRequests.end(),
                                               [](QPair<xcb_timestamp_t, bool> req) {
                                                   return req.second;
                                               });
    if (transfersFinished) {
        m_visit->sendFinished();
        Q_EMIT finish(this);
    }
    return transfersFinished;
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

static bool hasMimeName(const Mimes &mimes, const QString &name)
{
    return std::any_of(mimes.begin(), mimes.end(),
                       [name](const Mime &m) {
                           return m.first == name;
                       });
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

    // get types
    Mimes offers;
    if (!(data->data32[1] & 1)) {
        // message has only max 3 types (which are directly in data)
        for (size_t i = 0; i < 3; i++) {
            xcb_atom_t mimeAtom = data->data32[2 + i];
            const auto mimeStrings = atomToMimeTypes(mimeAtom);
            for (const auto &mime : mimeStrings) {
                if (!hasMimeName(offers, mime)) {
                    offers << Mime(mime, mimeAtom);
                }
            }
        }
    } else {
        // more than 3 types -> in window property
        getMimesFromWinProperty(offers);
    }

    Q_EMIT offersReceived(offers);
    return true;
}

void WlVisit::getMimesFromWinProperty(Mimes &offers)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    auto cookie = xcb_get_property(xcbConn,
                                   0,
                                   m_srcWindow,
                                   atoms->xdnd_type_list,
                                   XCB_GET_PROPERTY_TYPE_ANY,
                                   0, 0x1fffffff);

    auto *reply = xcb_get_property_reply(xcbConn, cookie, nullptr);
    if (reply == nullptr) {
        return;
    }
    if (reply->type != XCB_ATOM_ATOM || reply->value_len == 0) {
        // invalid reply value
        free(reply);
        return;
    }

    xcb_atom_t *mimeAtoms = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
    for (size_t i = 0; i < reply->value_len; ++i) {
        const auto mimeStrings = atomToMimeTypes(mimeAtoms[i]);
        for (const auto &mime : mimeStrings) {
            if (!hasMimeName(offers, mime)) {
                offers << Mime(mime, mimeAtoms[i]);
            }
        }
    }
    free(reply);
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
