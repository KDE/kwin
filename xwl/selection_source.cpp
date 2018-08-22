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
#include "selection_source.h"
#include "selection.h"
#include "transfer.h"

#include "atoms.h"
#include "wayland_server.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datasource.h>

#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/datasource_interface.h>

#include <unistd.h>

#include <xwayland_logging.h>

namespace KWin {
namespace Xwl {

SelectionSource::SelectionSource(Selection *sel)
    : QObject(sel),
      m_sel(sel),
      m_window(sel->window())
{
}

WlSource::WlSource(Selection *sel, KWayland::Server::DataDeviceInterface *ddi)
    : SelectionSource(sel),
      m_ddi(ddi)
{
    Q_ASSERT(ddi);
}

void WlSource::setDataSourceIface(KWayland::Server::DataSourceInterface *dsi)
{
    if (m_dsi == dsi) {
        return;
    }
    for (const auto &mime : dsi->mimeTypes()) {
        m_offers << mime;
    }
    m_offerCon = connect(dsi,
                         &KWayland::Server::DataSourceInterface::mimeTypeOffered,
                         this, &WlSource::receiveOffer);
    m_dsi = dsi;
}

void WlSource::receiveOffer(const QString &mime)
{
    m_offers << mime;
}

void WlSource::sendSelNotify(xcb_selection_request_event_t *event, bool success)
{
    Selection::sendSelNotify(event, success);
}

bool WlSource::handleSelRequest(xcb_selection_request_event_t *event)
{
    if (event->target == atoms->targets) {
        sendTargets(event);
    } else if (event->target == atoms->timestamp) {
        sendTimestamp(event);
    } else if (event->target == atoms->delete_atom) {
        sendSelNotify(event, true);
    } else {
        // try to send mime data
        if (!checkStartTransfer(event)) {
            sendSelNotify(event, false);
        }
    }
    return true;
}

void WlSource::sendTargets(xcb_selection_request_event_t *event)
{
    QVector<xcb_atom_t> targets;
    targets.resize(m_offers.size() + 2);
    targets[0] = atoms->timestamp;
    targets[1] = atoms->targets;

    size_t cnt = 2;
    for (const auto mime : m_offers) {
        targets[cnt] = Selection::mimeTypeToAtom(mime);
        cnt++;
    }

    xcb_change_property(kwinApp()->x11Connection(),
                        XCB_PROP_MODE_REPLACE,
                        event->requestor,
                        event->property,
                        XCB_ATOM_ATOM,
                        32, cnt, targets.data());
    sendSelNotify(event, true);
}

void WlSource::sendTimestamp(xcb_selection_request_event_t *event)
{
    const xcb_timestamp_t time = timestamp();
    xcb_change_property(kwinApp()->x11Connection(),
                        XCB_PROP_MODE_REPLACE,
                        event->requestor,
                        event->property,
                        XCB_ATOM_INTEGER,
                        32, 1, &time);

    sendSelNotify(event, true);
}

bool WlSource::checkStartTransfer(xcb_selection_request_event_t *event)
{
    // check interfaces available
    if (!m_ddi || !m_dsi) {
        return false;
    }

    const auto targets = Selection::atomToMimeTypes(event->target);
    if (targets.isEmpty()) {
        qCDebug(KWIN_XWL) << "Unknown selection atom. Ignoring request.";
        return false;
    }
    const auto firstTarget = targets[0];

    auto cmp = [firstTarget](const QString &b) {
        if (firstTarget == "text/uri-list") {
            // Wayland sources might announce the old mime or the new standard
            return firstTarget == b || b == "text/x-uri";
        }
        return firstTarget == b;
    };
    // check supported mimes
    const auto offers = m_dsi->mimeTypes();
    const auto mimeIt = std::find_if(offers.begin(), offers.end(), cmp);
    if (mimeIt == offers.end()) {
        // Requested Mime not supported. Not sending selection.
        return false;
    }

    int p[2];
    if (pipe(p) == -1) {
        qCWarning(KWIN_XWL) << "Pipe failed. Not sending selection.";
        return false;
    }

    m_dsi->requestData(*mimeIt, p[1]);
    waylandServer()->dispatch();

    Q_EMIT transferReady(new xcb_selection_request_event_t(*event), p[0]);
    return true;
}

X11Source::X11Source(Selection *sel, xcb_xfixes_selection_notify_event_t *event)
    : SelectionSource(sel),
      m_owner(event->owner)
{
    setTimestamp(event->timestamp);
}

void X11Source::getTargets()
{
    auto *xcbConn = kwinApp()->x11Connection();
    /* will lead to a selection request event for the new owner */
    xcb_convert_selection(xcbConn,
                          window(),
                          selection()->atom(),
                          atoms->targets,
                          atoms->wl_selection,
                          timestamp());
    xcb_flush(xcbConn);
}

using Mime = QPair<QString, xcb_atom_t>;

void X11Source::handleTargets()
{
    // receive targets
    auto *xcbConn = kwinApp()->x11Connection();
    xcb_get_property_cookie_t cookie = xcb_get_property(xcbConn,
                                                        1,
                                                        window(),
                                                        atoms->wl_selection,
                                                        XCB_GET_PROPERTY_TYPE_ANY,
                                                        0,
                                                        4096
                                                        );
    auto *reply = xcb_get_property_reply(xcbConn, cookie, NULL);
    if (reply == NULL) {
        return;
    }
    if (reply->type != XCB_ATOM_ATOM) {
        free(reply);
        return;
    }

    Mimes all;
    QVector<QString> add, rm;
    xcb_atom_t *value = static_cast<xcb_atom_t*>(xcb_get_property_value(reply));
    for (uint32_t i = 0; i < reply->value_len; i++) {
        if (value[i] == XCB_ATOM_NONE) {
            continue;
        }

        const auto mimeStrings = Selection::atomToMimeTypes(value[i]);
        if (mimeStrings.isEmpty()) {
            // TODO: this should never happen? assert?
            continue;
        }


        const auto mimeIt = std::find_if(m_offers.begin(), m_offers.end(),
                                           [value, i](const Mime &m)
                                                { return m.second == value[i]; });

        auto mimePair = Mime(mimeStrings[0], value[i]);
        if (mimeIt == m_offers.end()) {
            add << mimePair.first;
        } else {
            m_offers.removeAll(mimePair);
        }
        all << mimePair;
    }
    // all left in m_offers are not in the updated targets
    for (const auto mimePair : m_offers) {
        rm << mimePair.first;
    }
    m_offers = all;

    if (!add.isEmpty() || !rm.isEmpty()) {
        Q_EMIT offersChanged(add, rm);
    }

    free(reply);
}

void X11Source::setDataSource(KWayland::Client::DataSource *ds)
{
    Q_ASSERT(ds);
    if (m_ds) {
        delete m_ds;
    }
    m_ds = ds;

    std::for_each(m_offers.begin(), m_offers.end(),
                  [ds](const Mime &offer){
                      ds->offer(offer.first);
                  });
    connect(ds, &KWayland::Client::DataSource::sendDataRequested,
            this, &X11Source::startTransfer);
}

void X11Source::setOffers(const Mimes &offers)
{
    // TODO: share code with handleTargets and emit signals accordingly?
    m_offers = offers;
}

bool X11Source::handleSelNotify(xcb_selection_notify_event_t *event)
{
    if (event->requestor != window()) {
        return false;
    }
    if (event->selection != selection()->atom()) {
        return false;
    }
    if (event->property == XCB_ATOM_NONE) {
        qCWarning(KWIN_XWL) << "Incoming X selection conversion failed";
        return true;
    }
    if (event->target == atoms->targets) {
        handleTargets();
        return true;
    }
    return false;
}

void X11Source::startTransfer(const QString &mimeName, qint32 fd)
{
    const auto mimeIt = std::find_if(m_offers.begin(), m_offers.end(),
                                       [mimeName](const Mime &m)
                                            { return m.first == mimeName; });
    if (mimeIt == m_offers.end()) {
        qCDebug(KWIN_XWL) << "Sending X11 clipboard to Wayland failed: unsupported MIME.";
        close(fd);
        return;
    }

    Q_EMIT transferReady((*mimeIt).second, fd);
}


}
}
