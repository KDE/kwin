/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "transfer.h"

#include "databridge.h"
#include "xwayland.h"

#include "abstract_client.h"
#include "atoms.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datasource.h>

#include <KWaylandServer/datadevice_interface.h>
#include <KWaylandServer/datasource_interface.h>
#include <KWaylandServer/seat_interface.h>

#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <algorithm>
#include <unistd.h>

#include <xwayland_logging.h>

namespace KWin
{
namespace Xwl
{

// in Bytes: equals 64KB
static const uint32_t s_incrChunkSize = 63 * 1024;

Transfer::Transfer(xcb_atom_t selection, qint32 fd, xcb_timestamp_t timestamp, QObject *parent)
    : QObject(parent)
    , m_atom(selection)
    , m_fd(fd)
    , m_timestamp(timestamp)
{
}

void Transfer::createSocketNotifier(QSocketNotifier::Type type)
{
    delete m_notifier;
    m_notifier = new QSocketNotifier(m_fd, type, this);
}

void Transfer::clearSocketNotifier()
{
    delete m_notifier;
    m_notifier = nullptr;
}

void Transfer::timeout()
{
    if (m_timeout) {
        endTransfer();
    }
    m_timeout = true;
}

void Transfer::endTransfer()
{
    clearSocketNotifier();
    closeFd();
    Q_EMIT finished();
}

void Transfer::closeFd()
{
    if (m_fd < 0) {
        return;
    }
    close(m_fd);
    m_fd = -1;
}

TransferWltoX::TransferWltoX(xcb_atom_t selection, xcb_selection_request_event_t *request,
                             qint32 fd, QObject *parent)
    : Transfer(selection, fd, 0, parent)
    , m_request(request)
{
}

TransferWltoX::~TransferWltoX()
{
    delete m_request;
    m_request = nullptr;
}

void TransferWltoX::startTransferFromSource()
{
    createSocketNotifier(QSocketNotifier::Read);
    connect(socketNotifier(), &QSocketNotifier::activated, this,
            [this](int socket) {
                Q_UNUSED(socket);
                readWlSource();
            }
    );
}

int TransferWltoX::flushSourceData()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    xcb_change_property(xcbConn,
                        XCB_PROP_MODE_REPLACE,
                        m_request->requestor,
                        m_request->property,
                        m_request->target,
                        8,
                        m_chunks.first().first.size(),
                        m_chunks.first().first.data());
    xcb_flush(xcbConn);

    m_propertyIsSet = true;
    resetTimeout();

    const auto rm = m_chunks.takeFirst();
    return rm.first.size();
}

void TransferWltoX::startIncr()
{
    Q_ASSERT(m_chunks.size() == 1);

    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    uint32_t mask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes (xcbConn,
                                  m_request->requestor,
                                  XCB_CW_EVENT_MASK, mask);

    // spec says to make the available space larger
    const uint32_t chunkSpace = 1024 + s_incrChunkSize;
    xcb_change_property(xcbConn,
                        XCB_PROP_MODE_REPLACE,
                        m_request->requestor,
                        m_request->property,
                        atoms->incr,
                        32, 1, &chunkSpace);
    xcb_flush(xcbConn);

    setIncr(true);
    // first data will be flushed after the property has been deleted
    // again by the requestor
    m_flushPropertyOnDelete = true;
    m_propertyIsSet = true;
    Q_EMIT selectionNotify(m_request, true);
}

void TransferWltoX::readWlSource()
{
    if (m_chunks.size() == 0 ||
            m_chunks.last().second == s_incrChunkSize) {
        // append new chunk
        auto next = QPair<QByteArray, int>();
        next.first.resize(s_incrChunkSize);
        next.second = 0;
        m_chunks.append(next);
    }

    const auto oldLen = m_chunks.last().second;
    const auto avail = s_incrChunkSize - m_chunks.last().second;
    Q_ASSERT(avail > 0);

    ssize_t readLen = read(fd(), m_chunks.last().first.data() + oldLen, avail);
    if (readLen == -1) {
        qCWarning(KWIN_XWL) << "Error reading in Wl data.";

        // TODO: cleanup X side?
        endTransfer();
        return;
    }
    m_chunks.last().second = oldLen + readLen;

    if (readLen == 0) {
        // at the fd end - complete transfer now
        m_chunks.last().first.resize(m_chunks.last().second);

        if (incr()) {
            // incremental transfer is to be completed now
            m_flushPropertyOnDelete = true;
            if (!m_propertyIsSet) {
                // flush if target's property is not set at the moment
                flushSourceData();
            }
            clearSocketNotifier();
        } else {
            // non incremental transfer is to be completed now,
            // data can be transferred to X client via a single property set
            flushSourceData();
            Q_EMIT selectionNotify(m_request, true);
            endTransfer();
        }
    } else if (m_chunks.last().second == s_incrChunkSize) {
        // first chunk full, but not yet at fd end -> go incremental
        if (incr()) {
            m_flushPropertyOnDelete = true;
            if (!m_propertyIsSet) {
                // flush if target's property is not set at the moment
                flushSourceData();
            }
        } else {
            // starting incremental transfer
            startIncr();
        }
    }
    resetTimeout();
}

bool TransferWltoX::handlePropertyNotify(xcb_property_notify_event_t *event)
{
    if (event->window == m_request->requestor) {
        if (event->state == XCB_PROPERTY_DELETE &&
                event->atom == m_request->property) {
            handlePropertyDelete();
        }
        return true;
    }
    return false;
}

void TransferWltoX::handlePropertyDelete()
{
    if (!incr()) {
        // non-incremental transfer: nothing to do
        return;
    }
    m_propertyIsSet = false;

    if (m_flushPropertyOnDelete) {
        if (!socketNotifier() && m_chunks.isEmpty()) {
            // transfer complete
            xcb_connection_t *xcbConn = kwinApp()->x11Connection();

            uint32_t mask[] = {0};
            xcb_change_window_attributes (xcbConn,
                                          m_request->requestor,
                                          XCB_CW_EVENT_MASK, mask);

            xcb_change_property(xcbConn,
                                XCB_PROP_MODE_REPLACE,
                                m_request->requestor,
                                m_request->property,
                                m_request->target,
                                8, 0, nullptr);
            xcb_flush(xcbConn);
            m_flushPropertyOnDelete = false;
            endTransfer();
        } else {
            flushSourceData();
        }
    }
}

TransferXtoWl::TransferXtoWl(xcb_atom_t selection, xcb_atom_t target, qint32 fd,
                             xcb_timestamp_t timestamp, xcb_window_t parentWindow,
                             QObject *parent)
    : Transfer(selection, fd, timestamp, parent)
{
    // create transfer window
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    m_window = xcb_generate_id(xcbConn);
    const uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                   XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      m_window,
                      parentWindow,
                      0, 0,
                      10, 10,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      values);
    // convert selection
    xcb_convert_selection(xcbConn,
                          m_window,
                          selection,
                          target,
                          atoms->wl_selection,
                          timestamp);
    xcb_flush(xcbConn);
}

TransferXtoWl::~TransferXtoWl()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_destroy_window(xcbConn, m_window);
    xcb_flush(xcbConn);

    delete m_receiver;
    m_receiver = nullptr;
}

bool TransferXtoWl::handlePropertyNotify(xcb_property_notify_event_t *event)
{
    if (event->window == m_window) {
        if (event->state == XCB_PROPERTY_NEW_VALUE &&
                event->atom == atoms->wl_selection) {
            getIncrChunk();
        }
        return true;
    }
    return false;
}

bool TransferXtoWl::handleSelectionNotify(xcb_selection_notify_event_t *event)
{
    if (event->requestor != m_window) {
        return false;
    }
    if (event->selection != atom()) {
        return false;
    }
    if (event->property == XCB_ATOM_NONE) {
        qCWarning(KWIN_XWL) << "Incoming X selection conversion failed";
        return true;
    }
    if (event->target == atoms->targets) {
        qCWarning(KWIN_XWL) << "Received targets too late";
        // TODO: or allow it?
        return true;
    }
    if (m_receiver) {
        // second selection notify element - misbehaving source

        // TODO: cancel this transfer?
        return true;
    }

    if (event->target == atoms->netscape_url) {
        m_receiver = new NetscapeUrlReceiver;
    } else if (event->target == atoms->moz_url) {
        m_receiver = new MozUrlReceiver;
    } else {
        m_receiver = new DataReceiver;
    }
    startTransfer();
    return true;
}

void TransferXtoWl::startTransfer()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    auto cookie = xcb_get_property(xcbConn,
                                   1,
                                   m_window,
                                   atoms->wl_selection,
                                   XCB_GET_PROPERTY_TYPE_ANY,
                                   0,
                                   0x1fffffff
                                   );

    auto *reply = xcb_get_property_reply(xcbConn, cookie, nullptr);
    if (reply == nullptr) {
        qCWarning(KWIN_XWL) << "Can't get selection property.";
        endTransfer();
        return;
    }

    if (reply->type == atoms->incr) {
        setIncr(true);
        free(reply);
    } else {
        setIncr(false);
        // reply's ownership is transferred
        m_receiver->transferFromProperty(reply);
        dataSourceWrite();
    }
}

void TransferXtoWl::getIncrChunk()
{
    if (!incr()) {
        // source tries to sent incrementally, but did not announce it before
        return;
    }
    if (!m_receiver) {
        // receive mechanism has not yet been setup
        return;
    }
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    auto cookie = xcb_get_property(xcbConn,
                                   0,
                                   m_window,
                                   atoms->wl_selection,
                                   XCB_GET_PROPERTY_TYPE_ANY,
                                   0,
                                   0x1fffffff);

    auto *reply = xcb_get_property_reply(xcbConn, cookie, nullptr);
    if (!reply) {
        qCWarning(KWIN_XWL) << "Can't get selection property.";
        endTransfer();
        return;
    }

    if (xcb_get_property_value_length(reply) > 0) {
        // reply's ownership is transferred
        m_receiver->transferFromProperty(reply);
        dataSourceWrite();
    } else {
        // Transfer complete
        free(reply);
        endTransfer();
    }
}

DataReceiver::~DataReceiver()
{
    if (m_propertyReply) {
        free(m_propertyReply);
        m_propertyReply = nullptr;
    }
}

void DataReceiver::transferFromProperty(xcb_get_property_reply_t *reply)
{
    m_propertyStart = 0;
    m_propertyReply = reply;

    setData(static_cast<char *>(xcb_get_property_value(reply)),
            xcb_get_property_value_length(reply));
}

void DataReceiver::setData(const char *value, int length)
{
    // simply set data without copy
    m_data = QByteArray::fromRawData(value, length);
}

QByteArray DataReceiver::data() const
{
    return QByteArray::fromRawData(m_data.data() + m_propertyStart,
                                   m_data.size() - m_propertyStart);
}

void DataReceiver::partRead(int length)
{
    m_propertyStart += length;
    if (m_propertyStart == m_data.size()) {
        Q_ASSERT(m_propertyReply);
        free(m_propertyReply);
        m_propertyReply = nullptr;
    }
}

void NetscapeUrlReceiver::setData(const char *value, int length)
{
    auto origData = QByteArray::fromRawData(value, length);

    if (origData.indexOf('\n') == -1) {
        // there are no line breaks, not in Netscape url format or empty,
        // but try anyway
        setDataInternal(origData);
        return;
    }
    // remove every second line
    QByteArray data;
    int start = 0;
    bool remLine = false;
    while (start < length) {
        auto part = QByteArray::fromRawData(value + start, length - start);
        const int linebreak = part.indexOf('\n');
        if (linebreak == -1) {
            // no more linebreaks, end of work
            if (!remLine) {
                // append the rest
                data.append(part);
            }
            break;
        }
        if (remLine) {
            // no data to add, but add a linebreak for the next line
            data.append('\n');
        } else {
            // add data till before linebreak
            data.append(part.data(), linebreak);
        }
        remLine = !remLine;
        start = linebreak + 1;
    }
    setDataInternal(data);
}

void MozUrlReceiver::setData(const char *value, int length)
{
    // represent as QByteArray (guaranteed '\0'-terminated)
    const auto origData = QByteArray::fromRawData(value, length);

    // text/x-moz-url data is sent in utf-16 - copies the content
    // and converts it into 8 byte representation
    const auto byteData = QString::fromUtf16(reinterpret_cast<const char16_t *>(origData.data())).toLatin1();

    if (byteData.indexOf('\n') == -1) {
        // there are no line breaks, not in text/x-moz-url format or empty,
        // but try anyway
        setDataInternal(byteData);
        return;
    }
    // remove every second line
    QByteArray data;
    int start = 0;
    bool remLine = false;
    while (start < length) {
        auto part = QByteArray::fromRawData(byteData.data() + start, byteData.size() - start);
        const int linebreak = part.indexOf('\n');
        if (linebreak == -1) {
            // no more linebreaks, end of work
            if (!remLine) {
                // append the rest
                data.append(part);
            }
            break;
        }
        if (remLine) {
            // no data to add, but add a linebreak for the next line
            data.append('\n');
        } else {
            // add data till before linebreak
            data.append(part.data(), linebreak);
        }
        remLine = !remLine;
        start = linebreak + 1;
    }
    setDataInternal(data);
}

void TransferXtoWl::dataSourceWrite()
{
    QByteArray property = m_receiver->data();

    ssize_t len = write(fd(), property.constData(), property.size());
    if (len == -1) {
        qCWarning(KWIN_XWL) << "X11 to Wayland write error on fd:" << fd();
        endTransfer();
        return;
    }

    m_receiver->partRead(len);
    if (len == property.size()) {
        // property completely transferred
        if (incr()) {
            clearSocketNotifier();
            xcb_connection_t *xcbConn = kwinApp()->x11Connection();
            xcb_delete_property(xcbConn,
                                m_window,
                                atoms->wl_selection);
            xcb_flush(xcbConn);
        } else {
            // transfer complete
            endTransfer();
        }
    } else {
        if (!socketNotifier()) {
            createSocketNotifier(QSocketNotifier::Write);
            connect(socketNotifier(), &QSocketNotifier::activated, this,
                [this](int socket) {
                    Q_UNUSED(socket);
                    dataSourceWrite();
                }
            );
        }
    }
    resetTimeout();
}

} // namespace Xwl
} // namespace KWin
