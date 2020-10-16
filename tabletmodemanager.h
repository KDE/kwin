/*
    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

*/

#ifndef KWIN_TABLETMODEMANAGER_H
#define KWIN_TABLETMODEMANAGER_H

#include <QObject>
#include <kwinglobals.h>

namespace KWin {

class TabletModeManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.TabletModeManager")
    //assuming such a switch is not pluggable for now
    Q_PROPERTY(bool tabletModeAvailable READ isTabletModeAvailable NOTIFY tabletModeAvailableChanged)
    Q_PROPERTY(bool tabletMode READ isTablet NOTIFY tabletModeChanged)

public:
    ~TabletModeManager() override = default;

    void setTabletModeAvailable(bool detecting);
    bool isTabletModeAvailable() const;

    bool isTablet() const;
    void setIsTablet(bool tablet);

Q_SIGNALS:
    void tabletModeAvailableChanged(bool available);
    void tabletModeChanged(bool tabletMode);

private:
    void hasTabletModeInputChanged(bool set);

    bool m_tabletModeAvailable = false;
    bool m_isTabletMode = false;
    bool m_detecting = false;
    KWIN_SINGLETON_VARIABLE(TabletModeManager, s_manager)
};
}

#endif // KWIN_TABLETMODEMANAGER_H
