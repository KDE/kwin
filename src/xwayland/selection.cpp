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

#include "atoms.h"
#include "utils/xcbutils.h"
#include "workspace.h"
#include "x11window.h"

#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <QTimer>

namespace KWin
{
namespace Xwl
{

xcb_atom_t Selection::mimeTypeToAtom(const QString &mimeType)
{
    if (mimeType == QLatin1String("text/plain;charset=utf-8")) {
        return atoms->utf8_string;
    }
    if (mimeType == QLatin1String("text/plain")) {
        return atoms->text;
    }
    if (mimeType == QLatin1String("text/x-uri")) {
        return atoms->uri_list;
    }
    return mimeTypeToAtomLiteral(mimeType);
}

xcb_atom_t Selection::mimeTypeToAtomLiteral(const QString &mimeType)
{
    return Xcb::Atom(mimeType.toLatin1(), false, kwinApp()->x11Connection());
}

QString Selection::atomName(xcb_atom_t atom)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_get_atom_name_cookie_t nameCookie = xcb_get_atom_name(xcbConn, atom);
    xcb_get_atom_name_reply_t *nameReply = xcb_get_atom_name_reply(xcbConn, nameCookie, nullptr);
    if (!nameReply) {
        return QString();
    }

    const size_t length = xcb_get_atom_name_name_length(nameReply);
    QString name = QString::fromLatin1(xcb_get_atom_name_name(nameReply), length);
    free(nameReply);
    return name;
}

QStringList Selection::atomToMimeTypes(xcb_atom_t atom)
{
    QStringList mimeTypes;

    if (atom == atoms->utf8_string) {
        mimeTypes << QStringLiteral("text/plain;charset=utf-8");
    } else if (atom == atoms->text) {
        mimeTypes << QStringLiteral("text/plain");
    } else if (atom == atoms->uri_list) {
        mimeTypes << QStringLiteral("text/uri-list")
                  << QStringLiteral("text/x-uri");
    } else {
        mimeTypes << atomName(atom);
    }
    return mimeTypes;
}

Selection::Selection(xcb_atom_t atom, QObject *parent)
    : QObject(parent)
    , m_atom(atom)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    m_window = xcb_generate_id(kwinApp()->x11Connection());
    m_requestorWindow = m_window;
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
    if (event->owner == m_window && m_waylandSource) {
        // When we claim a selection we must use XCB_TIME_CURRENT,
        // grab the actual timestamp here to answer TIMESTAMP requests
        // correctly
        m_waylandSource->setTimestamp(event->timestamp);
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
    } u;
    memset(&u, 0, sizeof(u));
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
    xcb_flush(xcbConn);
}

void Selection::registerXfixes()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    const uint32_t mask = XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;
    xcb_xfixes_select_selection_input(xcbConn,
                                      m_window,
                                      m_atom,
                                      mask);
    xcb_flush(xcbConn);
}

void Selection::setWlSource(WlSource *source)
{
    if (m_waylandSource) {
        m_waylandSource->deleteLater();
        m_waylandSource = nullptr;
    }
    delete m_xSource;
    m_xSource = nullptr;
    if (source) {
        m_waylandSource = source;
        connect(source, &WlSource::transferReady, this, &Selection::startTransferToX);
    }
}

void Selection::createX11Source(xcb_xfixes_selection_notify_event_t *event)
{
    if (!event || event->owner == XCB_WINDOW_NONE) {
        x11OfferLost();
        setWlSource(nullptr);
        return;
    }
    setWlSource(nullptr);

    m_xSource = new X11Source(this, event);
    connect(m_xSource, &X11Source::offersChanged, this, &Selection::x11OffersChanged);
    connect(m_xSource, &X11Source::transferReady, this, &Selection::startTransferToWayland);
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
        m_disownPending = true;
        xcb_set_selection_owner(xcbConn,
                                XCB_WINDOW_NONE,
                                m_atom,
                                m_timestamp);
    }
    xcb_flush(xcbConn);
}

void Selection::overwriteRequestorWindow(xcb_window_t window)
{
    Q_ASSERT(m_xSource);
    if (window == XCB_WINDOW_NONE) {
        // reset
        window = m_window;
    }
    m_requestorWindow = window;
    m_xSource->setRequestor(window);
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

void Selection::startTransferToWayland(xcb_atom_t target, qint32 fd)
{
    // create new x to wl data transfer object
    auto *transfer = new TransferXtoWl(m_atom, target, fd, m_xSource->timestamp(), m_requestorWindow, this);
    m_xToWlTransfers << transfer;

    connect(transfer, &TransferXtoWl::finished, this, [this, transfer]() {
        Q_EMIT transferFinished(transfer->timestamp());
        transfer->deleteLater();
        m_xToWlTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });
    startTimeoutTransfersTimer();
}

void Selection::startTransferToX(xcb_selection_request_event_t *event, qint32 fd)
{
    // create new wl to x data transfer object
    auto *transfer = new TransferWltoX(m_atom, event, fd, this);

    connect(transfer, &TransferWltoX::selectionNotify, this, &Selection::sendSelectionNotify);
    connect(transfer, &TransferWltoX::finished, this, [this, transfer]() {
        Q_EMIT transferFinished(transfer->timestamp());

        // TODO: serialize? see comment below.
        //        const bool wasActive = (transfer == m_wlToXTransfers[0]);
        transfer->deleteLater();
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
    for (TransferXtoWl *transfer : std::as_const(m_xToWlTransfers)) {
        transfer->timeout();
    }
    for (TransferWltoX *transfer : std::as_const(m_wlToXTransfers)) {
        transfer->timeout();
    }
}

} // namespace Xwl
} // namespace KWin

#include "moc_selection.cpp"
