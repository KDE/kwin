/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "selection.h"
#include "databridge.h"
#include "selection_source.h"
#include "transfer.h"
#include "xwayland_logging.h"

#include "atoms.h"
#include "utils/xcbutils.h"
#include "workspace.h"
#include "x11window.h"

#include <unistd.h>
#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <QTimer>

namespace KWin
{
namespace Xwl
{

Selection::Selection(xcb_atom_t atom, QObject *parent)
    : QObject(parent)
    , m_atom(atom)
{
}

bool Selection::handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    if (event->window != m_window) {
        return false;
    }
    if (event->selection != m_atom) {
        return false;
    }

    const xcb_window_t previousOwner = m_owner;
    m_owner = event->owner;

    if (m_owner == XCB_WINDOW_NONE) {
        if (previousOwner == m_window) {
            return true;
        }
    }

    if (event->owner == m_window) {
        // When we claim a selection we must use XCB_TIME_CURRENT,
        // grab the actual timestamp here to answer TIMESTAMP requests
        // correctly
        m_timestamp = event->timestamp;
        return true;
    }

    // Being here means some other X window has claimed the selection.
    doHandleXfixesNotify(event);
    return true;
}

bool Selection::filterEvent(xcb_generic_event_t *event)
{
    switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
    case XCB_SELECTION_NOTIFY:
        return handleSelectionNotify(reinterpret_cast<xcb_selection_notify_event_t *>(event));
    case XCB_PROPERTY_NOTIFY:
        return handlePropertyNotify(reinterpret_cast<xcb_property_notify_event_t *>(event));
    case XCB_SELECTION_REQUEST:
        return handleSelectionRequest(reinterpret_cast<xcb_selection_request_event_t *>(event));
    case XCB_CLIENT_MESSAGE:
        return handleClientMessage(reinterpret_cast<xcb_client_message_event_t *>(event));
    default:
        if (event->response_type == Xcb::Extensions::self()->fixesSelectionNotifyEvent()) {
            return handleXfixesNotify(reinterpret_cast<xcb_xfixes_selection_notify_event_t *>(event));
        }
        return false;
    }
}

void Selection::sendSelectionNotify(xcb_selection_request_event_t *event, bool success)
{
    // Every X11 event is 32 bytes (see man xcb_send_event), so XCB will copy
    // 32 unconditionally. Use a union to ensure we don't disclose stack memory.
    union {
        xcb_selection_notify_event_t notify;
        char buffer[32];
    } u{};
    static_assert(sizeof(u.notify) < 32, "wouldn't need the union otherwise");
    u.notify.response_type = XCB_SELECTION_NOTIFY;
    u.notify.sequence = 0;
    u.notify.time = event->time;
    u.notify.requestor = event->requestor;
    u.notify.selection = event->selection;
    u.notify.target = event->target;
    u.notify.property = success ? event->property : xcb_atom_t(XCB_ATOM_NONE);

    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_send_event(xcbConn, 0, event->requestor, XCB_EVENT_MASK_NO_EVENT, (const char *)&u);
}

void Selection::registerXfixes()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    const uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;
    xcb_xfixes_select_selection_input(xcbConn,
                                      m_window,
                                      m_atom,
                                      mask);
}

void Selection::setWlSource(WlSource *source)
{
    if (!source) {
        m_waylandSource.reset();
        return;
    }

    m_xSource.reset();

    m_waylandSource.reset(source);
    connect(m_waylandSource.get(), &WlSource::transferReady, this, &Selection::startTransferToX);
}

void Selection::createX11Source(xcb_xfixes_selection_notify_event_t *event)
{
    if (!event || event->owner == XCB_WINDOW_NONE) {
        x11OfferLost();
        m_xSource.reset();
        return;
    }

    m_waylandSource.reset();
    m_timestamp = event->timestamp;

    m_xSource = std::make_unique<X11Source>(this);
    connect(m_xSource.get(), &X11Source::targetsReceived, this, &Selection::x11TargetsReceived);
    connect(m_xSource.get(), &X11Source::transferRequested, this, &Selection::startTransferToWayland);
}

void Selection::ownSelection(bool own)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    if (own) {
        xcb_set_selection_owner(xcbConn,
                                m_window,
                                m_atom,
                                XCB_TIME_CURRENT_TIME);
    } else {
        if (m_owner == m_window) {
            xcb_set_selection_owner(xcbConn,
                                    XCB_WINDOW_NONE,
                                    m_atom,
                                    m_timestamp);
        }
    }
}

bool Selection::handleSelectionRequest(xcb_selection_request_event_t *event)
{
    if (event->selection != m_atom) {
        return false;
    }

    if (qobject_cast<X11Window *>(workspace()->activeWindow()) == nullptr) {
        // Receiving Wayland selection not allowed when no Xwayland surface active
        // filter the event, but don't act upon it
        sendSelectionNotify(event, false);
        return true;
    }

    if (m_window != event->owner || !m_waylandSource) {
        if (event->time < m_timestamp) {
            // cancel earlier attempts at receiving a selection
            // TODO: is this for sure without problems?
            sendSelectionNotify(event, false);
            return true;
        }
        return false;
    }
    return m_waylandSource->handleSelectionRequest(event);
}

bool Selection::handleSelectionNotify(xcb_selection_notify_event_t *event)
{
    if (m_xSource && m_xSource->handleSelectionNotify(event)) {
        return true;
    }
    for (TransferXtoWl *transfer : std::as_const(m_xToWlTransfers)) {
        if (transfer->handleSelectionNotify(event)) {
            return true;
        }
    }
    return false;
}

bool Selection::handlePropertyNotify(xcb_property_notify_event_t *event)
{
    for (TransferXtoWl *transfer : std::as_const(m_xToWlTransfers)) {
        if (transfer->handlePropertyNotify(event)) {
            return true;
        }
    }
    for (TransferWltoX *transfer : std::as_const(m_wlToXTransfers)) {
        if (transfer->handlePropertyNotify(event)) {
            return true;
        }
    }
    return false;
}

void Selection::startTransferToWayland(const QString &mimeType, qint32 fd)
{
    const xcb_atom_t mimeAtom = Xcb::mimeTypeToAtom(mimeType);
    if (mimeAtom == XCB_ATOM_NONE) {
        qCDebug(KWIN_XWL) << "Sending X11 clipboard to Wayland failed: unsupported MIME.";
        close(fd);
        return;
    }

    auto *transfer = new TransferXtoWl(m_atom, mimeAtom, fd, m_timestamp, m_window, this);
    m_xToWlTransfers << transfer;

    connect(transfer, &TransferXtoWl::finished, this, [this, transfer]() {
        transfer->deleteLater();
        m_xToWlTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });
    startTimeoutTransfersTimer();
}

void Selection::startTransferToX(xcb_selection_request_event_t *event, qint32 fd)
{
    auto *transfer = new TransferWltoX(m_atom, event, fd, this);

    connect(transfer, &TransferWltoX::finished, this, [this, transfer]() {
        transfer->deleteLater();
        m_wlToXTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });

    m_wlToXTransfers.append(transfer);
    transfer->startTransferFromSource();
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
    // A transfer can be removed from the corresponding list on timeout, so avoid iterating directly
    // on m_xToWlTransfers and m_wlToXTransfers.

    const auto xToWaylandTransfers = m_xToWlTransfers;
    for (TransferXtoWl *transfer : xToWaylandTransfers) {
        transfer->timeout();
    }

    const auto waylandToXTransfers = m_wlToXTransfers;
    for (TransferWltoX *transfer : waylandToXTransfers) {
        transfer->timeout();
    }
}

} // namespace Xwl
} // namespace KWin

#include "moc_selection.cpp"
