/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipboard.h"

#include "databridge.h"
#include "selection_source.h"
#include "transfer.h"
#include "xwayland.h"

#include "x11client.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datasource.h>

#include <KWaylandServer/datadevice_interface.h>
#include <KWaylandServer/datasource_interface.h>
#include <KWaylandServer/seat_interface.h>

#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <xwayland_logging.h>

namespace KWin
{
namespace Xwl
{

Clipboard::Clipboard(xcb_atom_t atom, QObject *parent)
    : Selection(atom, parent)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    const uint32_t clipboardValues[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                   XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      window(),
                      kwinApp()->x11RootWindow(),
                      0, 0,
                      10, 10,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      clipboardValues);
    registerXfixes();
    xcb_flush(xcbConn);

    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::selectionChanged,
            this, &Clipboard::wlSelectionChanged);

    connect(DataBridge::self()->dataDeviceIface(), &KWaylandServer::DataDeviceInterface::selectionChanged, this, [](KWaylandServer::DataSourceInterface *selection) {
        waylandServer()->seat()->setSelection(selection);
    });
}

void Clipboard::wlSelectionChanged(KWaylandServer::AbstractDataSource *dsi)
{
    if (m_waitingForTargets) {
        return;
    }

    if (dsi && !ownsSelection(dsi)) {
        // Wayland native client provides new selection
        if (!m_checkConnection) {
            m_checkConnection = connect(workspace(), &Workspace::clientActivated,
                                        this, [this](AbstractClient *ac) {
                                            Q_UNUSED(ac);
                                            checkWlSource();
                                        });
        }
        // remove previous source so checkWlSource() can create a new one
        setWlSource(nullptr);
    }
    checkWlSource();
}

bool Clipboard::ownsSelection(KWaylandServer::AbstractDataSource *dsi) const
{
    return dsi->client() == DataBridge::self()->dataDeviceIface()->client()->client();
}

void Clipboard::checkWlSource()
{
    if (m_waitingForTargets) {
        return;
    }

    auto dsi = waylandServer()->seat()->selection();
    auto removeSource = [this] {
        if (wlSource()) {
            setWlSource(nullptr);
            ownSelection(false);
        }
    };

    // Wayland source gets created when:
    // - the Wl selection exists,
    // - its source is not Xwayland,
    // - a client is active,
    // - this client is an Xwayland one.
    //
    // Otherwise the Wayland source gets destroyed to shield
    // against snooping X clients.

    if (!dsi || (DataBridge::self()->dataDeviceIface()->client()->client() == dsi->client())) {
        // Xwayland source or no source
        disconnect(m_checkConnection);
        m_checkConnection = QMetaObject::Connection();
        removeSource();
        return;
    }
    if (!workspace()->activeClient() || !workspace()->activeClient()->inherits("KWin::X11Client")) {
        // no active client or active client is Wayland native
        removeSource();
        return;
    }
    // Xwayland client is active and we need a Wayland source
    if (wlSource()) {
        // source already exists, nothing more to do
        return;
    }
    auto *wls = new WlSource(this);
    setWlSource(wls);
    if (dsi) {
        wls->setDataSourceIface(dsi);
    }
    ownSelection(true);
}

void Clipboard::doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    const AbstractClient *client = workspace()->activeClient();
    if (!qobject_cast<const X11Client *>(client)) {
        // clipboard is only allowed to be acquired when Xwayland has focus
        // TODO: can we make this stronger (window id comparison)?
        createX11Source(nullptr);
        return;
    }

    createX11Source(event);

    if (X11Source *source = x11Source()) {
        source->getTargets();
        m_waitingForTargets = true;
    } else {
        qCWarning(KWIN_XWL) << "Could not create a source from" << event << Qt::hex << (event ? event->owner : -1);
    }
}

void Clipboard::x11OffersChanged(const QStringList &added, const QStringList &removed)
{
    m_waitingForTargets = false;
    X11Source *source = x11Source();
    if (!source) {
        qCWarning(KWIN_XWL) << "offers changed when not having an X11Source!?";
        return;
    }

    const Mimes offers = source->offers();

    if (!offers.isEmpty()) {
        if (!source->dataSource() || !removed.isEmpty()) {
            // create new Wl DataSource if there is none or when types
            // were removed (Wl Data Sources can only add types)
            KWayland::Client::DataDeviceManager *dataDeviceManager =
                waylandServer()->internalDataDeviceManager();
            KWayland::Client::DataSource *dataSource =
                dataDeviceManager->createDataSource(source);

            // also offers directly the currently available types
            source->setDataSource(dataSource);
            DataBridge::self()->dataDevice()->setSelection(0, dataSource);
        } else if (auto *dataSource = source->dataSource()) {
            for (const QString &mime : added) {
                dataSource->offer(mime);
            }
        }
    } else {
        KWaylandServer::AbstractDataSource *currentSelection = waylandServer()->seat()->selection();
        if (currentSelection && !ownsSelection(currentSelection)) {
            waylandServer()->seat()->setSelection(nullptr);
        }
    }

    waylandServer()->internalClientConection()->flush();
    waylandServer()->dispatch();
}

} // namespace Xwl
} // namespace KWin
