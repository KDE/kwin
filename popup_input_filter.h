/*
 * Copyright 2017  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef KWIN_POPUP_INPUT_FILTER
#define KWIN_POPUP_INPUT_FILTER

#include "input.h"

#include <QObject>
#include <QVector>

namespace KWin
{
class Toplevel;
class XdgShellClient;

class PopupInputFilter : public QObject, public InputEventFilter
{
    Q_OBJECT
public:
    explicit PopupInputFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
private:
    void handleClientAdded(Toplevel *client);
    void handleClientRemoved(Toplevel *client);
    void disconnectClient(Toplevel *client);
    void cancelPopups();

    QVector<Toplevel*> m_popupClients;
};
}

#endif
