/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "primary.h"

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

Primary::Primary(xcb_atom_t atom, QObject *parent)
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

    connect(waylandServer()->seat(), &SeatInterface::primarySelectionChanged, this, &Primary::checkWlSource);
}

Primary::~Primary() = default;

bool Primary::ownsSelection(AbstractDataSource *dsi) const
{
    return dsi && dsi == m_primarySelectionSource.get();
}

void Primary::checkWlSource()
{
    auto currentSelection = waylandServer()->seat()->primarySelection();

    if (!currentSelection || ownsSelection(currentSelection)) {
        if (wlSource()) {
            setWlSource(nullptr);
            ownSelection(false);
        }
        return;
    }

    auto *source = new WlSource(this);
    source->setDataSourceIface(currentSelection);
    setWlSource(source);
    ownSelection(true);
}

void Primary::doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    const Window *window = workspace()->activeWindow();
    if (!qobject_cast<const X11Window *>(window)) {
        // clipboard is only allowed to be acquired when Xwayland has focus
        // TODO: can we make this stronger (window id comparison)?
        createX11Source(nullptr);
        return;
    }

    createX11Source(event);

    if (X11Source *source = x11Source()) {
        source->getTargets();
    } else {
        qCWarning(KWIN_XWL) << "Could not create a source from" << event << Qt::hex << (event ? event->owner : -1);
    }
}

void Primary::x11OfferLost()
{
    m_primarySelectionSource.reset();
}

void Primary::x11OffersChanged(const QStringList &added, const QStringList &removed)
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
        // we keep the old selection around because setPrimarySelection needs it to be still alive
        std::swap(m_primarySelectionSource, newSelection);
        waylandServer()->seat()->setPrimarySelection(m_primarySelectionSource.get(), waylandServer()->display()->nextSerial());
    } else {
        AbstractDataSource *currentSelection = waylandServer()->seat()->primarySelection();
        if (!ownsSelection(currentSelection)) {
            waylandServer()->seat()->setPrimarySelection(nullptr, waylandServer()->display()->nextSerial());
            m_primarySelectionSource.reset();
        }
    }
}

} // namespace Xwl
} // namespace KWin

#include "moc_primary.cpp"
