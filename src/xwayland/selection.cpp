/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "selection.h"
#include "atoms.h"
#include "selection_source.h"
#include "transfer.h"
#include "utils/pipe.h"
#include "utils/xcbutils.h"
#include "wayland/abstract_data_source.h"
#include "workspace.h"
#include "x11window.h"
#include "xwayland_logging.h"

#include <fcntl.h>
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

void Selection::setWlSource(AbstractDataSource *source)
{
    if (m_waylandSource == source) {
        return;
    }

    m_waylandSource = source;
    if (source) {
        m_xSource.reset();
    }

    ownSelection(m_waylandSource != nullptr);
}

void Selection::createX11Source(xcb_xfixes_selection_notify_event_t *event)
{
    if (!event || event->owner == XCB_WINDOW_NONE) {
        x11OfferLost();
        m_xSource.reset();
        return;
    }

    m_waylandSource = nullptr;
    m_timestamp = event->timestamp;

    m_xSource = std::make_unique<X11Source>(this);
    connect(m_xSource.get(), &X11Source::targetsReceived, this, &Selection::x11TargetsReceived);
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

    if (event->target == atoms->targets) {
        sendTargets(event);
    } else if (event->target == atoms->timestamp) {
        sendTimestamp(event);
    } else if (event->target == atoms->delete_atom) {
        sendSelectionNotify(event, true);
    } else {
        if (!startTransferToX(event)) {
            sendSelectionNotify(event, false);
        }
    }

    return true;
}

void Selection::sendTargets(xcb_selection_request_event_t *event)
{
    Q_ASSERT(m_waylandSource);

    const QStringList mimeTypes = m_waylandSource->mimeTypes();

    QList<xcb_atom_t> targets;
    targets.reserve(mimeTypes.size() + 2);

    targets.append(atoms->timestamp);
    targets.append(atoms->targets);

    for (const auto &mime : mimeTypes) {
        targets.append(Xcb::mimeTypeToAtom(mime));
    }

    xcb_change_property(kwinApp()->x11Connection(),
                        XCB_PROP_MODE_REPLACE,
                        event->requestor,
                        event->property,
                        XCB_ATOM_ATOM,
                        32, targets.size(), targets.data());

    sendSelectionNotify(event, true);
}

void Selection::sendTimestamp(xcb_selection_request_event_t *event)
{
    Q_ASSERT(m_waylandSource);

    xcb_change_property(kwinApp()->x11Connection(),
                        XCB_PROP_MODE_REPLACE,
                        event->requestor,
                        event->property,
                        XCB_ATOM_INTEGER,
                        32, 1, &m_timestamp);

    sendSelectionNotify(event, true);
}

bool Selection::handleSelectionNotify(xcb_selection_notify_event_t *event)
{
    if (m_xSource && m_xSource->handleSelectionNotify(event)) {
        return true;
    }

    // A transfer can be removed from the m_xToWlTransfers list while processing a SelectionNotify event,
    // so avoid iterating directly on m_xToWlTransfers.
    const auto xToWlTransfers = m_xToWlTransfers;
    for (TransferXtoWl *transfer : xToWlTransfers) {
        if (transfer->handleSelectionNotify(event)) {
            return true;
        }
    }
    return false;
}

bool Selection::handlePropertyNotify(xcb_property_notify_event_t *event)
{
    // A transfer can be removed from the corresponding list while processing a PropertyNotify event,
    // so avoid iterating directly on m_xToWlTransfers and m_wlToXTransfers.

    const auto xToWlTransfers = m_xToWlTransfers;
    for (TransferXtoWl *transfer : xToWlTransfers) {
        if (transfer->handlePropertyNotify(event)) {
            return true;
        }
    }

    const auto wlToXTransfers = m_wlToXTransfers;
    for (TransferWltoX *transfer : wlToXTransfers) {
        if (transfer->handlePropertyNotify(event)) {
            return true;
        }
    }

    return false;
}

void Selection::startTransferToWayland(const QString &mimeType, FileDescriptor fd)
{
    Q_ASSERT(m_xSource);
    const xcb_atom_t mimeAtom = Xcb::mimeTypeToAtom(mimeType);
    if (mimeAtom == XCB_ATOM_NONE) {
        qCDebug(KWIN_XWL) << "Sending X11 clipboard to Wayland failed: unsupported MIME.";
        return;
    }

    if (fcntl(fd.get(), F_SETFL, O_NONBLOCK) == -1) {
        qCWarning(KWIN_XWL) << "Failed to set O_NONBLOCK flag for the write endpoint of an X11 to Wayland transfer pipe:" << strerror(errno);
    }

    auto *transfer = new TransferXtoWl(m_atom, mimeAtom, std::move(fd), m_timestamp, m_window, this);
    m_xToWlTransfers << transfer;

    connect(transfer, &TransferXtoWl::finished, this, [this, transfer]() {
        transfer->deleteLater();
        m_xToWlTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });
    startTimeoutTransfersTimer();
}

static QString selectMimeType(const QStringList &interested, const QStringList &available)
{
    for (const QString &mimeType : interested) {
        if (available.contains(mimeType)) {
            return mimeType;
        }
    }
    return QString();
}

bool Selection::startTransferToX(xcb_selection_request_event_t *event)
{
    Q_ASSERT(m_waylandSource);

    const auto targets = Xcb::atomToMimeTypes(event->target);
    if (targets.isEmpty()) {
        qCDebug(KWIN_XWL) << "Unknown selection atom. Ignoring request.";
        return false;
    }

    const QString mimeType = selectMimeType(targets, m_waylandSource->mimeTypes());
    if (mimeType.isEmpty()) {
        return false;
    }

    std::optional<Pipe> pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        qCWarning(KWIN_XWL) << "Pipe failed, not sending selection:" << strerror(errno);
        return false;
    }

    if (fcntl(pipe->readEndpoint.get(), F_SETFL, O_NONBLOCK) == -1) {
        qCWarning(KWIN_XWL) << "Failed to set O_NONBLOCK flag for the read endpoint of a Wayland to X11 transfer pipe:" << strerror(errno);
    }

    m_waylandSource->requestData(mimeType, std::move(pipe->writeEndpoint));

    auto transfer = new TransferWltoX(*event, std::move(pipe->readEndpoint), this);
    m_wlToXTransfers.append(transfer);

    connect(transfer, &TransferWltoX::finished, this, [this, transfer]() {
        transfer->deleteLater();
        m_wlToXTransfers.removeOne(transfer);
        endTimeoutTransfersTimer();
    });

    transfer->startTransferFromSource();
    startTimeoutTransfersTimer();

    return true;
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
