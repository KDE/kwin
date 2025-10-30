/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QObject>

#include <xcb/xcb.h>

struct xcb_xfixes_selection_notify_event_t;

class QTimer;

namespace KWin
{
namespace Xwl
{
class TransferWltoX;
class TransferXtoWl;
class WlSource;
class X11Source;

/**
 * Base class representing generic X selections and their respective
 * Wayland counter-parts.
 *
 * The class needs to be subclassed and adjusted according to the
 * selection, but provides common functionality to be expected of all
 * selections.
 *
 * A selection should exist through the whole runtime of an Xwayland
 * session.
 *
 * Independently of each other the class holds the currently active
 * source instance and active transfers relative to the represented
 * selection.
 */
class Selection : public QObject
{
    Q_OBJECT

public:
    static void sendSelectionNotify(xcb_selection_request_event_t *event, bool success);

    // on selection owner changes by X clients (Xwl -> Wl)
    bool handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event);
    bool filterEvent(xcb_generic_event_t *event);

    xcb_atom_t atom() const
    {
        return m_atom;
    }
    xcb_window_t window() const
    {
        return m_window;
    }
    void overwriteRequestorWindow(xcb_window_t window);

protected:
    Selection(xcb_atom_t atom, QObject *parent);
    void registerXfixes();

    virtual void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) = 0;
    virtual void x11OfferLost() = 0;
    virtual void x11TargetsReceived(const QStringList &mimeTypes) = 0;

    virtual bool handleClientMessage(xcb_client_message_event_t *event)
    {
        return false;
    }
    // sets the current provider of the selection
    void setWlSource(WlSource *source);
    WlSource *wlSource() const
    {
        return m_waylandSource.get();
    }
    void createX11Source(xcb_xfixes_selection_notify_event_t *event);
    X11Source *x11Source() const
    {
        return m_xSource.get();
    }
    // must be called in order to provide data from Wl to X
    void ownSelection(bool own);

private:
    bool handleSelectionRequest(xcb_selection_request_event_t *event);
    bool handleSelectionNotify(xcb_selection_notify_event_t *event);
    bool handlePropertyNotify(xcb_property_notify_event_t *event);

    void startTransferToWayland(const QString &mimeType, qint32 fd);
    void startTransferToX(xcb_selection_request_event_t *event, qint32 fd);

    // Timeout transfers, which have become inactive due to client errors.
    void timeoutTransfers();
    void startTimeoutTransfersTimer();
    void endTimeoutTransfersTimer();

    xcb_atom_t m_atom = XCB_ATOM_NONE;
    xcb_window_t m_owner = XCB_WINDOW_NONE;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_window_t m_requestorWindow = XCB_WINDOW_NONE;
    xcb_timestamp_t m_timestamp;

    // Active source, if any. Only one of them at max can exist
    // at the same time.
    std::unique_ptr<WlSource> m_waylandSource;
    std::unique_ptr<X11Source> m_xSource;

    // active transfers
    QList<TransferWltoX *> m_wlToXTransfers;
    QList<TransferXtoWl *> m_xToWlTransfers;
    QTimer *m_timeoutTransfers = nullptr;

    Q_DISABLE_COPY(Selection)
};

} // namespace Xwl
} // namespace KWin
