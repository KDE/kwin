/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_XWL_TRANSFER
#define KWIN_XWL_TRANSFER

#include <QObject>
#include <QSocketNotifier>
#include <QVector>

#include <xcb/xcb.h>

namespace KWayland {
namespace Client {
class DataDevice;
class DataSource;
}
namespace Server {
class DataDeviceInterface;
}
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

    virtual bool handlePropNotify(xcb_property_notify_event_t *event) = 0;
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
    QSocketNotifier* socketNotifier() const {
        return m_sn;
    }
private:
    void closeFd();

    xcb_atom_t m_atom;
    qint32 m_fd;
    xcb_timestamp_t m_timestamp = XCB_CURRENT_TIME;

    QSocketNotifier *m_sn = nullptr;
    bool m_incr = false;
    bool m_timeout = false;
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
    ~TransferWltoX();

    void startTransferFromSource();
    bool handlePropNotify(xcb_property_notify_event_t *event) override;

Q_SIGNALS:
    void selNotify(xcb_selection_request_event_t *event, bool success);

private:
    void startIncr();
    void readWlSource();
    int flushSourceData();
    void handlePropDelete();

    xcb_selection_request_event_t *m_request = nullptr;

    /* contains all received data portioned in chunks
     * TODO: explain second QPair component
     */
    QVector<QPair<QByteArray, int> > chunks;

    bool propertyIsSet = false;
    bool flushPropOnDelete = false;
};

/**
 * Helper class for X to Wl transfers
 */
class DataReceiver
{
public:
    virtual ~DataReceiver();

    void transferFromProperty(xcb_get_property_reply_t *reply);


    virtual void setData(char *value, int length);
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
    void setData(char *value, int length) override;
};

/**
 * Compatibility receiver for clients only
 * supporting the text/x-moz-url scheme (Chromium on own drags)
 */
class MozUrlReceiver : public DataReceiver
{
public:
    void setData(char *value, int length) override;
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
    ~TransferXtoWl();

    bool handleSelNotify(xcb_selection_notify_event_t *event);
    bool handlePropNotify(xcb_property_notify_event_t *event) override;

private:
    void dataSourceWrite();
    void startTransfer();
    void getIncrChunk();

    xcb_window_t m_window;
    DataReceiver *m_receiver = nullptr;
};

}
}

#endif
