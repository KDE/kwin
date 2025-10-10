/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "selection_source.h"
#include "datasource.h"
#include "selection.h"
#include "transfer.h"

#include "atoms.h"
#include "wayland/abstract_data_source.h"

#include <fcntl.h>
#include <span>
#include <unistd.h>

#include <xwayland_logging.h>

namespace KWin
{
namespace Xwl
{

SelectionSource::SelectionSource(Selection *selection)
    : QObject(selection)
    , m_selection(selection)
    , m_window(selection->window())
{
}

WlSource::WlSource(AbstractDataSource *dataSource, Selection *selection)
    : SelectionSource(selection)
    , m_dsi(dataSource)
    , m_offers(dataSource->mimeTypes())
{
}

void WlSource::sendSelectionNotify(xcb_selection_request_event_t *event, bool success)
{
    Selection::sendSelectionNotify(event, success);
}

bool WlSource::handleSelectionRequest(xcb_selection_request_event_t *event)
{
    if (event->target == atoms->targets) {
        sendTargets(event);
    } else if (event->target == atoms->timestamp) {
        sendTimestamp(event);
    } else if (event->target == atoms->delete_atom) {
        sendSelectionNotify(event, true);
    } else {
        // try to send mime data
        if (!checkStartTransfer(event)) {
            sendSelectionNotify(event, false);
        }
    }
    return true;
}

void WlSource::sendTargets(xcb_selection_request_event_t *event)
{
    QList<xcb_atom_t> targets;
    targets.reserve(m_offers.size() + 2);

    targets.append(atoms->timestamp);
    targets.append(atoms->targets);

    for (const auto &mime : std::as_const(m_offers)) {
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

void WlSource::sendTimestamp(xcb_selection_request_event_t *event)
{
    const xcb_timestamp_t time = timestamp();
    xcb_change_property(kwinApp()->x11Connection(),
                        XCB_PROP_MODE_REPLACE,
                        event->requestor,
                        event->property,
                        XCB_ATOM_INTEGER,
                        32, 1, &time);

    sendSelectionNotify(event, true);
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

bool WlSource::checkStartTransfer(xcb_selection_request_event_t *event)
{
    if (!m_dsi) {
        return false;
    }

    const auto targets = Xcb::atomToMimeTypes(event->target);
    if (targets.isEmpty()) {
        qCDebug(KWIN_XWL) << "Unknown selection atom. Ignoring request.";
        return false;
    }

    const QString mimeType = selectMimeType(targets, m_dsi->mimeTypes());
    if (mimeType.isEmpty()) {
        return false;
    }

    int p[2];
    if (pipe2(p, O_CLOEXEC) == -1) {
        qCWarning(KWIN_XWL) << "Pipe failed. Not sending selection.";
        return false;
    }

    m_dsi->requestData(mimeType, p[1]);

    Q_EMIT transferReady(new xcb_selection_request_event_t(*event), p[0]);
    return true;
}

X11Source::X11Source(Selection *selection, xcb_xfixes_selection_notify_event_t *event)
    : SelectionSource(selection)
    , m_dataSource(std::make_unique<XwlDataSource>())
{
    setTimestamp(event->timestamp);
}

X11Source::~X11Source()
{
}

void X11Source::getTargets()
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    /* will lead to a selection request event for the new owner */
    xcb_convert_selection(xcbConn,
                          window(),
                          selection()->atom(),
                          atoms->targets,
                          atoms->wl_selection,
                          timestamp());
    xcb_flush(xcbConn);
}

static bool isSpecialSelectionTarget(xcb_atom_t atom)
{
    return atom == atoms->targets || atom == atoms->multiple || atom == atoms->timestamp || atom == atoms->save_targets;
}

void X11Source::handleTargets()
{
    // receive targets
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();
    xcb_get_property_cookie_t cookie = xcb_get_property(xcbConn,
                                                        1,
                                                        window(),
                                                        atoms->wl_selection,
                                                        XCB_GET_PROPERTY_TYPE_ANY,
                                                        0,
                                                        4096);
    auto *reply = xcb_get_property_reply(xcbConn, cookie, nullptr);
    if (!reply) {
        qCDebug(KWIN_XWL) << "Failed to get selection property";
        return;
    }
    if (reply->type != XCB_ATOM_ATOM) {
        qCDebug(KWIN_XWL) << "Wrong reply type";
        free(reply);
        return;
    }

    QStringList mimeTypes;
    xcb_atom_t *value = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
    for (xcb_atom_t value : std::span(value, reply->value_len)) {
        if (!isSpecialSelectionTarget(value)) {
            mimeTypes += Xcb::atomToMimeTypes(value);
        }
    }

    Q_EMIT targetsReceived(mimeTypes);

    free(reply);
}

bool X11Source::handleSelectionNotify(xcb_selection_notify_event_t *event)
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

} // namespace Xwl
} // namespace KWin

#include "moc_selection_source.cpp"
