/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "utils/filedescriptor.h"

#include <QList>
#include <QObject>

#include <xcb/xcb.h>

struct xcb_xfixes_selection_notify_event_t;

class QTimer;

namespace KWin
{
class AbstractDataSource;

namespace Xwl
{
class TransferWltoX;
class TransferXtoWl;
class XwlDataSource;

/**
 * The Selection class represents an X selection. Three selections are supported by us: the clipboard,
 * the primary selection, and the XDND selection.
 *
 * At any moment in time, a selection can have either a Wayland source or an X11 source.
 *
 * If a Wayland source is attached to the selection, we will claim the ownership of the selection. If
 * the selection is claimed by another X client, a proxy data source will be created and set as the current
 * data source.
 *
 * Note that with the clipboard and the primary selection, an X11 client can read Wayland data sources
 * only if it is focused. X11 clients can write to the clipboard and the primary selection regardless of
 * whether they are focused or not.
 */
class Selection : public QObject
{
    Q_OBJECT

public:
    static void sendSelectionNotify(xcb_selection_request_event_t *event, bool success);

    bool filterEvent(xcb_generic_event_t *event);

    xcb_atom_t atom() const
    {
        return m_atom;
    }
    xcb_window_t window() const
    {
        return m_window;
    }
    xcb_timestamp_t timestamp() const
    {
        return m_timestamp;
    }
    void setTimestamp(xcb_timestamp_t timestamp)
    {
        m_timestamp = timestamp;
    }

    bool ownsDataSource(AbstractDataSource *dataSource) const;

    void startTransferToWayland(const QString &mimeType, FileDescriptor fd);
    bool startTransferToX(xcb_selection_request_event_t *event);

protected:
    Selection(xcb_atom_t atom, QObject *parent);
    void registerXfixes();

    // Called when the selection is claimed or disowned by somebody else, not us.
    virtual void selectionClaimed(xcb_xfixes_selection_notify_event_t *event);
    virtual void selectionDisowned();

    void requestTargets();
    virtual void targetsReceived(const QStringList &mimeTypes);

    virtual bool handleClientMessage(xcb_client_message_event_t *event)
    {
        return false;
    }
    // sets the current provider of the selection
    void setWlSource(AbstractDataSource *source);
    AbstractDataSource *wlSource() const
    {
        return m_waylandSource;
    }
    XwlDataSource *x11Source() const
    {
        return m_xSource.get();
    }
    // must be called in order to provide data from Wl to X
    void ownSelection(bool own);

    bool handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event);
    bool handleSelectionRequest(xcb_selection_request_event_t *event);
    bool handleSelectionNotify(xcb_selection_notify_event_t *event);
    bool handlePropertyNotify(xcb_property_notify_event_t *event);
    bool handleSelectionTargets(xcb_selection_notify_event_t *event);

    void sendTargets(xcb_selection_request_event_t *event);
    void sendTimestamp(xcb_selection_request_event_t *event);

    // Timeout transfers, which have become inactive due to client errors.
    void timeoutTransfers();
    void startTimeoutTransfersTimer();
    void endTimeoutTransfersTimer();

    xcb_atom_t m_atom = XCB_ATOM_NONE;
    xcb_window_t m_owner = XCB_WINDOW_NONE;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_timestamp_t m_timestamp = 0;

    // Active source, if any. Only one of them at max can exist
    // at the same time.
    AbstractDataSource *m_waylandSource = nullptr;
    std::unique_ptr<XwlDataSource> m_xSource;

    // active transfers
    QList<TransferWltoX *> m_wlToXTransfers;
    QList<TransferXtoWl *> m_xToWlTransfers;
    QTimer *m_timeoutTransfers = nullptr;

    Q_DISABLE_COPY(Selection)
};

} // namespace Xwl
} // namespace KWin
