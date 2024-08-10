/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipboard.h"

#include "datasource.h"
#include "selection_source.h"

#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

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

    const uint32_t clipboardValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
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

    connect(waylandServer()->seat(), &SeatInterface::selectionChanged,
            this, &Clipboard::wlSelectionChanged);
}

void Clipboard::wlSelectionChanged(AbstractDataSource *dsi)
{
    if (!ownsSelection(dsi)) {
        // Wayland native window provides new selection
        if (!m_checkConnection) {
            m_checkConnection = connect(workspace(), &Workspace::windowActivated,
                                        this, &Clipboard::checkWlSource);
        }
        // remove previous source so checkWlSource() can create a new one
        setWlSource(nullptr);
    }
    checkWlSource();
}

bool Clipboard::ownsSelection(AbstractDataSource *dsi) const
{
    return dsi && dsi == m_selectionSource.get();
}

void Clipboard::checkWlSource()
{
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
    // - a window is active,
    // - this window is an Xwayland one.
    //
    // Otherwise the Wayland source gets destroyed to shield
    // against snooping X windows.

    if (!dsi || ownsSelection(dsi)) {
        // Xwayland source or no source
        disconnect(m_checkConnection);
        m_checkConnection = QMetaObject::Connection();
        removeSource();
        return;
    }
    if (!qobject_cast<KWin::X11Window *>(workspace()->activeWindow())) {
        // no active window or active window is Wayland native
        removeSource();
        return;
    }
    // Xwayland window is active and we need a Wayland source
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
    createX11Source(event);

    if (X11Source *source = x11Source()) {
        source->getTargets();
    } else {
        qCWarning(KWIN_XWL) << "Could not create a source from" << event << Qt::hex << (event ? event->owner : -1);
    }
}

void Clipboard::x11OfferLost()
{
    m_selectionSource.reset();
}

void Clipboard::x11OffersChanged(const QStringList &added, const QStringList &removed)
{
    X11Source *source = x11Source();
    if (!source) {
        qCWarning(KWIN_XWL) << "offers changed when not having an X11Source!?";
        return;
    }

    const Mimes offers = source->offers();

    if (!offers.isEmpty()) {
        QStringList mimeTypes;
        mimeTypes.reserve(offers.size());
        std::transform(offers.begin(), offers.end(), std::back_inserter(mimeTypes), [](const Mimes::value_type &pair) {
            return pair.first;
        });
        auto newSelection = std::make_unique<XwlDataSource>();
        newSelection->setMimeTypes(mimeTypes);
        connect(newSelection.get(), &XwlDataSource::dataRequested, source, &X11Source::startTransfer);
        // we keep the old selection around because setSelection needs it to be still alive
        std::swap(m_selectionSource, newSelection);
        waylandServer()->seat()->setSelection(m_selectionSource.get(), waylandServer()->display()->nextSerial());
    } else {
        AbstractDataSource *currentSelection = waylandServer()->seat()->selection();
        if (!ownsSelection(currentSelection)) {
            waylandServer()->seat()->setSelection(nullptr, waylandServer()->display()->nextSerial());
            m_selectionSource.reset();
        }
    }
}

} // namespace Xwl
} // namespace KWin

#include "moc_clipboard.cpp"
