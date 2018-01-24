/*
 * Copyright 2018 Marco Martin <mart@kde.org>
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

#ifndef KWIN_TABLETMODEMANAGER_H
#define KWIN_TABLETMODEMANAGER_H

#include <QObject>
#include <kwinglobals.h>

namespace KWin {

class TabletModeInputEventSpy;

class TabletModeManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.TabletModeManager")
    //assuming such a switch is not pluggable for now
    Q_PROPERTY(bool tabletModeAvailable READ isTabletModeAvailable NOTIFY tabletModeAvailableChanged)
    Q_PROPERTY(bool tabletMode READ isTablet NOTIFY tabletModeChanged)

public:
    ~TabletModeManager() = default;

    bool isTabletModeAvailable() const;

    bool isTablet() const;
    void setIsTablet(bool tablet);

Q_SIGNALS:
    void tabletModeAvailableChanged(bool available);
    void tabletModeChanged(bool tabletMode);

private:
    bool m_tabletModeAvailable = false;
    bool m_isTabletMode = false;
    TabletModeInputEventSpy *m_spy;
    KWIN_SINGLETON_VARIABLE(TabletModeManager, s_manager)
};
}

#endif // KWIN_TABLETMODEMANAGER_H
