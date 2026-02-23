/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipboard.h"
#include "datasource.h"
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
    const uint32_t clipboardValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
    m_window = xcb_generate_id(kwinApp()->x11Connection());
    xcb_create_window(kwinApp()->x11Connection(),
                      XCB_COPY_FROM_PARENT,
                      m_window,
                      kwinApp()->x11RootWindow(),
                      0, 0,
                      10, 10,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      clipboardValues);
    registerXfixes();

    connect(waylandServer()->seat(), &SeatInterface::selectionChanged, this, &Clipboard::onSelectionChanged);
    connect(workspace(), &Workspace::windowActivated, this, &Clipboard::onActiveWindowChanged);
}

Clipboard::~Clipboard()
{
    // If m_xSource is the current selection, we do not want onSelectionChanged() to get called
    // while the Clipboard is already partially destroyed.
    disconnect(waylandServer()->seat(), &SeatInterface::selectionChanged, this, &Clipboard::onSelectionChanged);
    m_xSource.reset();
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
    if (!currentSelection || ownsDataSource(currentSelection)) {
        setWlSource(nullptr);
        return;
    }

    setWlSource(currentSelection);
}

void Clipboard::onActiveWindowChanged()
{
    auto currentSelection = waylandServer()->seat()->selection();
    if (!currentSelection) {
        return;
    }

    // If the current selection is owned by an X11 client => do nothing. If the selection is
    // owned by a Wayland client, X11 clients can access it only when they are focused.
    if (!ownsDataSource(currentSelection)) {
        if (x11ClientsCanAccessSelection()) {
            setWlSource(currentSelection);
        } else {
            setWlSource(nullptr);
        }
    }
}

void Clipboard::selectionDisowned()
{
    m_xSource.reset();
}

void Clipboard::selectionClaimed(xcb_xfixes_selection_notify_event_t *event)
{
    requestTargets();
}

void Clipboard::targetsReceived(const QStringList &mimeTypes)
{
    auto newSelection = std::make_unique<XwlDataSource>(this);
    newSelection->setMimeTypes(mimeTypes);

    // we keep the old selection around because setSelection needs it to be still alive
    std::swap(m_xSource, newSelection);
    waylandServer()->seat()->setSelection(m_xSource.get(), waylandServer()->display()->nextSerial());
}

} // namespace Xwl
} // namespace KWin

#include "moc_clipboard.cpp"
