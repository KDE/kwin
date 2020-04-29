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
#ifndef KWIN_XWL_CLIPBOARD
#define KWIN_XWL_CLIPBOARD

#include "selection.h"

namespace KWaylandServer
{
class DataDeviceInterface;
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
    void wlSelectionChanged(KWaylandServer::DataDeviceInterface *ddi);
    /**
     * Check the current state of the selection and if a source needs
     * to be created or destroyed.
     */
    void checkWlSource();

    QMetaObject::Connection m_checkConnection;

    Q_DISABLE_COPY(Clipboard)
};

} // namespace Xwl
} // namespace KWin

#endif
