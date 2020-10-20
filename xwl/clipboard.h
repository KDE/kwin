/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_CLIPBOARD
#define KWIN_XWL_CLIPBOARD

#include "selection.h"

namespace KWaylandServer
{
class AbstractDataSource;
}

namespace KWin
{
namespace Xwl
{

/**
 * Represents the X clipboard, which is on Wayland side just called
 * @e selection.
 */
class Clipboard : public Selection
{
    Q_OBJECT

public:
    Clipboard(xcb_atom_t atom, QObject *parent);

private:
    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OffersChanged(const QStringList &added, const QStringList &removed) override;
    /**
     * React to Wl selection change.
     */
    void wlSelectionChanged(KWaylandServer::AbstractDataSource *dsi);
    /**
     * Check the current state of the selection and if a source needs
     * to be created or destroyed.
     */
    void checkWlSource();

    /**
     * Returns of dsi is managed by our data bridge
     */
    bool ownsSelection(KWaylandServer::AbstractDataSource *dsi) const;

    QMetaObject::Connection m_checkConnection;

    Q_DISABLE_COPY(Clipboard)
    bool m_waitingForTargets = false;
};

} // namespace Xwl
} // namespace KWin

#endif
