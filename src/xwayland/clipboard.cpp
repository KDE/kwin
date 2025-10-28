/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipboard.h"

#include "datasource.h"
#include "selection_source.h"

#include "main.h"
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

    connect(waylandServer()->seat(), &SeatInterface::selectionChanged, this, &Clipboard::onSelectionChanged);
    connect(workspace(), &Workspace::windowActivated, this, &Clipboard::onActiveWindowChanged);
}

bool Clipboard::ownsSelection(AbstractDataSource *dsi) const
{
    return dsi && dsi == m_selectionSource.get();
}

bool Clipboard::x11ClientsCanAccessSelection() const
{
    return qobject_cast<X11Window *>(workspace()->activeWindow());
}

void Clipboard::onSelectionChanged()
{
    if (!x11ClientsCanAccessSelection()) {
        return;
    }

    auto currentSelection = waylandServer()->seat()->selection();
    if (!currentSelection || ownsSelection(currentSelection)) {
        if (wlSource()) {
            setWlSource(nullptr);
            ownSelection(false);
        }
        return;
    }

    setWlSource(new WlSource(currentSelection, this));
    ownSelection(true);
}

void Clipboard::onActiveWindowChanged()
{
    auto currentSelection = waylandServer()->seat()->selection();
    if (!currentSelection) {
        return;
    }

    // If the current selection is owned by an X11 client => do nothing. If the selection is
    // owned by a Wayland client, X11 clients can access it only when they are focused.
    if (!ownsSelection(currentSelection)) {
        if (x11ClientsCanAccessSelection()) {
            setWlSource(new WlSource(currentSelection, this));
            ownSelection(true);
        } else {
            if (wlSource()) {
                setWlSource(nullptr);
                ownSelection(false);
            }
        }
    }
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

void Clipboard::x11TargetsReceived(const QStringList &mimeTypes)
{
    X11Source *source = x11Source();
    if (!source) {
        qCWarning(KWIN_XWL) << "offers changed when not having an X11Source!?";
        return;
    }

    auto newSelection = std::make_unique<XwlDataSource>();
    newSelection->setMimeTypes(mimeTypes);
    connect(newSelection.get(), &XwlDataSource::dataRequested, source, &X11Source::transferRequested);
    // we keep the old selection around because setSelection needs it to be still alive
    std::swap(m_selectionSource, newSelection);
    waylandServer()->seat()->setSelection(m_selectionSource.get(), waylandServer()->display()->nextSerial());
}

} // namespace Xwl
} // namespace KWin

#include "moc_clipboard.cpp"
