/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_TRANSFER
#define KWIN_XWL_TRANSFER

#include <QObject>
#include <QSocketNotifier>
#include <QVector>

#include <xcb/xcb.h>

namespace KWayland
{
namespace Client
{
class DataDevice;
class DataSource;
}
}
namespace KWaylandServer
{
class DataDeviceInterface;
}

namespace KWin
{
namespace Xwl
{

/**
 * Represents for an arbitrary selection a data transfer between
 * sender and receiver.
 *
 * Lives for the duration of the transfer and must be cleaned up
 * externally afterwards. For that the owner should connect to the
 * @c finished() signal.
 */
class Transfer : public QObject
{
    Q_OBJECT

public:
    Transfer(xcb_atom_t selection,
             qint32 fd,
             xcb_timestamp_t timestamp,
             QObject *parent = nullptr);

    virtual bool handlePropertyNotify(xcb_property_notify_event_t *event) = 0;
    void timeout();
    xcb_timestamp_t timestamp() const {
        return m_timestamp;
    }

Q_SIGNALS:
    void finished();

protected:
    void endTransfer();

    xcb_atom_t atom() const {
        return m_atom;
    }
    qint32 fd() const {
        return m_fd;
    }

    void setIncr(bool set) {
        m_incr = set;
    }
    bool incr() const {
        return m_incr;
    }
    void resetTimeout() {
        m_timeout = false;
    }
    void createSocketNotifier(QSocketNotifier::Type type);
    void clearSocketNotifier();
    QSocketNotifier *socketNotifier() const {
        return m_notifier;
    }
private:
    void closeFd();

    xcb_atom_t m_atom;
    qint32 m_fd;
    xcb_timestamp_t m_timestamp = XCB_CURRENT_TIME;

    QSocketNotifier *m_notifier = nullptr;
    bool m_incr = false;
    bool m_timeout = false;

    Q_DISABLE_COPY(Transfer)
};

/**
 * Represents a transfer from a Wayland native source to an X window.
 */
class TransferWltoX : public Transfer
{
    Q_OBJECT

public:
    TransferWltoX(xcb_atom_t selection,
                  xcb_selection_request_event_t *request,
                  qint32 fd,
                  QObject *parent = nullptr);
    ~TransferWltoX() override;

    void startTransferFromSource();
    bool handlePropertyNotify(xcb_property_notify_event_t *event) override;

Q_SIGNALS:
    void selectionNotify(xcb_selection_request_event_t *event, bool success);

private:
    void startIncr();
    void readWlSource();
    int flushSourceData();
    void handlePropertyDelete();

    xcb_selection_request_event_t *m_request = nullptr;

    /* contains all received data portioned in chunks
     * TODO: explain second QPair component
     */
    QVector<QPair<QByteArray, int> > m_chunks;

    bool m_propertyIsSet = false;
    bool m_flushPropertyOnDelete = false;

    Q_DISABLE_COPY(TransferWltoX)
};

/**
 * Helper class for X to Wl transfers.
 */
class DataReceiver
{
public:
    virtual ~DataReceiver();

    void transferFromProperty(xcb_get_property_reply_t *reply);


    virtual void setData(const char *value, int length);
    QByteArray data() const;

    void partRead(int length);

protected:
    void setDataInternal(QByteArray data) {
        m_data = data;
    }

private:
    xcb_get_property_reply_t *m_propertyReply = nullptr;
    int m_propertyStart = 0;
    QByteArray m_data;
};

/**
 * Compatibility receiver for clients only
 * supporting the NETSCAPE_URL scheme (Firefox)
 */
class NetscapeUrlReceiver : public DataReceiver
{
public:
    void setData(const char *value, int length) override;
};

/**
 * Compatibility receiver for clients only
 * supporting the text/x-moz-url scheme (Chromium on own drags)
 */
class MozUrlReceiver : public DataReceiver
{
public:
    void setData(const char *value, int length) override;
};

/**
 * Represents a transfer from an X window to a Wayland native client.
 */
class TransferXtoWl : public Transfer
{
    Q_OBJECT

public:
    TransferXtoWl(xcb_atom_t selection,
                  xcb_atom_t target,
                  qint32 fd,
                  xcb_timestamp_t timestamp, xcb_window_t parentWindow,
                  QObject *parent = nullptr);
    ~TransferXtoWl() override;

    bool handleSelectionNotify(xcb_selection_notify_event_t *event);
    bool handlePropertyNotify(xcb_property_notify_event_t *event) override;

private:
    void dataSourceWrite();
    void startTransfer();
    void getIncrChunk();

    xcb_window_t m_window;
    DataReceiver *m_receiver = nullptr;

    Q_DISABLE_COPY(TransferXtoWl)
};

} // namespace Xwl
} // namespace KWin

#endif
