/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#include "selection.h"
#include "databridge.h"
#include "selection_source.h"
#include "transfer.h"

#include "atoms.h"
#include "workspace.h"
#include "client.h"

#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <QTimer>

namespace KWin {
namespace Xwl {

xcb_atom_t Selection::mimeTypeToAtom(const QString &mimeType)
{
    if (mimeType == QLatin1String("text/plain;charset=utf-8")) {
        return atoms->utf8_string;
    } else if (mimeType == QLatin1String("text/plain")) {
        return atoms->text;
    } else if (mimeType == QLatin1String("text/x-uri")) {
        return atoms->uri_list;
    }
    return mimeTypeToAtomLiteral(mimeType);
}

xcb_atom_t Selection::mimeTypeToAtomLiteral(const QString &mimeType)
{
    return Xcb::Atom(mimeType.toLatin1(), false, kwinApp()->x11Connection());
}

QStringList Selection::atomToMimeTypes(xcb_atom_t atom)
{
    QStringList mimeTypes;

    if (atom == atoms->utf8_string) {
        mimeTypes << QString::fromLatin1("text/plain;charset=utf-8");
    } else if (atom == atoms->text) {
        mimeTypes << QString::fromLatin1("text/plain");
    } else if (atom == atoms->uri_list) {
        mimeTypes << "text/uri-list" << "text/x-uri";
    } else {
        auto *xcbConn = kwinApp()->x11Connection();
        xcb_get_atom_name_cookie_t nameCookie = xcb_get_atom_name(xcbConn, atom);
        xcb_get_atom_name_reply_t *nameReply = xcb_get_atom_name_reply(xcbConn, nameCookie, NULL);
        if (nameReply == NULL) {
            return QStringList();
        }

        size_t len = xcb_get_atom_name_name_length(nameReply);
        char *name = xcb_get_atom_name_name(nameReply);
        mimeTypes << QString::fromLatin1(name, len);
        free(nameReply);
    }
    return mimeTypes;
}

Selection::Selection(xcb_atom_t atom, QObject *parent)
    : QObject(parent),
      m_atom(atom)
{
    auto *xcbConn = kwinApp()->x11Connection();
    m_window = xcb_generate_id(kwinApp()->x11Connection());
    xcb_flush(xcbConn);
}

bool Selection::handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    if (event->window != m_window) {
        return false;
    }
    if (event->selection != m_atom) {
        return false;
    }
    if (m_disownPending) {
        // notify of our own disown - ignore it
        m_disownPending = false;
        return true;
    }
    if (event->owner == m_window && m_wlSrc) {
        // When we claim a selection we must use XCB_TIME_CURRENT,
        // grab the actual timestamp here to answer TIMESTAMP requests
        // correctly
        m_wlSrc->setTimestamp(event->timestamp);
        m_timestamp = event->timestamp;
        return true;
    }

    // Being here means some other X window has claimed the selection.
    delete m_xSrc;
    m_xSrc = nullptr;
    const auto *ac = workspace()->activeClient();
    if (!ac || !ac->inherits("KWin::Client")) {
        // selections are only allowed to be acquired when Xwayland has focus
        // TODO: can we make this stronger (window id comparision)?
        return true;
    }
    doHandleXfixesNotify(event);
    return true;
}

bool Selection::filterEvent(xcb_generic_event_t *event)
{
    switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
    case XCB_SELECTION_NOTIFY:
        if (handleSelNotify(reinterpret_cast<xcb_selection_notify_event_t*>(event))) {
            return true;
        }
        Q_FALLTHROUGH();
    case XCB_PROPERTY_NOTIFY:
        if (handlePropNotify(reinterpret_cast<xcb_property_notify_event_t*>(event))) {
            return true;
        }
        Q_FALLTHROUGH();
    case XCB_SELECTION_REQUEST:
        if (handleSelRequest(reinterpret_cast<xcb_selection_request_event_t*>(event))) {
            return true;
        }
        Q_FALLTHROUGH();
    case XCB_CLIENT_MESSAGE:
        if (handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(event))) {
            return true;
        }
        Q_FALLTHROUGH();
    default:
        return false;
    }
}

void Selection::sendSelNotify(xcb_selection_request_event_t *event, bool success)
{
    xcb_selection_notify_event_t notify;
    notify.response_type = XCB_SELECTION_NOTIFY;
    notify.sequence = 0;
    notify.time = event->time;
    notify.requestor = event->requestor;
    notify.selection = event->selection;
    notify.target = event->target;
    notify.property = success ? event->property : (xcb_atom_t)XCB_ATOM_NONE;

    auto *xcbConn = kwinApp()->x11Connection();
    xcb_send_event(xcbConn,
                   0,
                   event->requestor,
                   XCB_EVENT_MASK_NO_EVENT,
                   (const char *)&notify);
    xcb_flush(xcbConn);
}

void Selection::registerXfixes()
{
    auto *xcbConn = kwinApp()->x11Connection();
    const uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;
    xcb_xfixes_select_selection_input(kwinApp()->x11Connection(),
                                      m_window,
                                      m_atom,
                                      mask);
    xcb_flush(xcbConn);
}

void Selection::setWlSource(WlSource *src)
{
    delete m_wlSrc;
    delete m_xSrc;
    m_wlSrc = nullptr;
    m_xSrc = nullptr;
    if (src) {
        m_wlSrc = src;
        connect(src, &WlSource::transferReady, this, &Selection::startTransferToX);
    }
}

void Selection::createX11Source(xcb_xfixes_selection_notify_event_t *event)
{
    delete m_wlSrc;
    delete m_xSrc;
    m_wlSrc = nullptr;
    m_xSrc = nullptr;
    if (event->owner == XCB_WINDOW_NONE) {
        return;
    }
    m_xSrc = new X11Source(this, event);

    connect(m_xSrc, &X11Source::offersChanged, this, &Selection::x11OffersChanged);
    connect(m_xSrc, &X11Source::transferReady, this, &Selection::startTransferToWayland);
}

void Selection::ownSelection(bool own)
{
    auto *xcbConn = kwinApp()->x11Connection();
    if (own) {
        xcb_set_selection_owner(xcbConn,
                                m_window,
                                m_atom,
                                XCB_TIME_CURRENT_TIME);
    } else {
        m_disownPending = true;
        xcb_set_selection_owner(xcbConn,
                                XCB_WINDOW_NONE,
                                m_atom,
                                m_timestamp);
    }
    xcb_flush(xcbConn);
}

bool Selection::handleSelRequest(xcb_selection_request_event_t *event)
{
    if (event->selection != m_atom) {
        return false;
    }

    if (qobject_cast<Client *>(workspace()->activeClient()) == nullptr) {
        // Receiving Wayland selection not allowed when no Xwayland surface active
        // filter the event, but don't act upon it
        sendSelNotify(event, false);
        return true;
    }

    if (m_window != event->owner || !m_wlSrc) {
        if (event->time < m_timestamp) {
            // cancel earlier attempts at receiving a selection
            // TODO: is this for sure without problems?
            sendSelNotify(event, false);
            return true;
        }
        return false;
    }
    return m_wlSrc->handleSelRequest(event);
}

bool Selection::handleSelNotify(xcb_selection_notify_event_t *event)
{
    if (m_xSrc && m_xSrc->handleSelNotify(event)) {
        return true;
    }
    for (auto *transfer : m_xToWlTransfers) {
        if (transfer->handleSelNotify(event)) {
            return true;
        }
    }
    return false;
}

bool Selection::handlePropNotify(xcb_property_notify_event_t *event)
{
    for (auto *transfer : m_xToWlTransfers) {
        if (transfer->handlePropNotify(event)) {
            return true;
        }
    }
    for (auto *transfer : m_wlToXTransfers) {
        if (transfer->handlePropNotify(event)) {
            return true;
        }
    }
    return false;
}

void Selection::startTransferToWayland(xcb_atom_t target, qint32 fd)
{
    // create new x to wl data transfer object
    auto *transfer = new TransferXtoWl(m_atom, target, fd, m_xSrc->timestamp(), m_window, this);
    m_xToWlTransfers << transfer;

    connect(transfer, &TransferXtoWl::finished, this, [this, transfer]() {
        Q_EMIT transferFinished(transfer->timestamp());
        delete transfer;
        m_xToWlTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });
    startTimeoutTransfersTimer();
}

void Selection::startTransferToX(xcb_selection_request_event_t *event, qint32 fd)
{
    // create new wl to x data transfer object
    auto *transfer = new TransferWltoX(m_atom, event, fd, this);

    connect(transfer, &TransferWltoX::selNotify, this, &Selection::sendSelNotify);
    connect(transfer, &TransferWltoX::finished, this, [this, transfer]() {
        Q_EMIT transferFinished(transfer->timestamp());

        // TODO: serialize? see comment below.
//        const bool wasActive = (transfer == m_wlToXTransfers[0]);
        delete transfer;
        m_wlToXTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
//        if (wasActive && !m_wlToXTransfers.isEmpty()) {
//            m_wlToXTransfers[0]->startTransferFromSource();
//        }
    });

    // add it to list of queued transfers
    m_wlToXTransfers.append(transfer);

    // TODO: Do we need to serialize the transfers, or can we do
    //       them in parallel as we do it right now?
    transfer->startTransferFromSource();
//    if (m_wlToXTransfers.size() == 1) {
//        transfer->startTransferFromSource();
//    }
    startTimeoutTransfersTimer();
}

void Selection::startTimeoutTransfersTimer()
{
    if (m_timeoutTransfers) {
        return;
    }
    m_timeoutTransfers = new QTimer(this);
    connect(m_timeoutTransfers, &QTimer::timeout, this, &Selection::timeoutTransfers);
    m_timeoutTransfers->start(5000);
}

void Selection::endTimeoutTransfersTimer()
{
    if (m_xToWlTransfers.isEmpty() && m_wlToXTransfers.isEmpty()) {
        delete m_timeoutTransfers;
        m_timeoutTransfers = nullptr;
    }
}

void Selection::timeoutTransfers()
{
    for (auto *transfer : m_xToWlTransfers) {
        transfer->timeout();
    }
    for (auto *transfer : m_wlToXTransfers) {
        transfer->timeout();
    }
}

}
}
