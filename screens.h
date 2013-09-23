/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_SCREENS_H
#define KWIN_SCREENS_H

// KWin includes
#include <kwinglobals.h>
// KDE includes
#include <KDE/KConfig>
// Qt includes
#include <QObject>
#include <QRect>
#include <QTimer>

class QDesktopWidget;

namespace KWin
{
class Client;

class Screens : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int current READ current WRITE setCurrent)
    Q_PROPERTY(bool currentFollowsMouse READ isCurrentFollowsMouse WRITE setCurrentFollowsMouse)

public:
    virtual ~Screens();
    /**
     * @internal
     **/
    void setConfig(KSharedConfig::Ptr config);
    int count() const;
    int current() const;
    void setCurrent(int current);
    /**
    * Called e.g. when a user clicks on a window, set current screen to be the screen
    * where the click occurred
    */
    void setCurrent(const QPoint &pos);
    /**
    * Check whether a client moved completely out of what's considered the current screen,
    * if yes, set a new active screen.
    */
    void setCurrent(const Client *c);
    bool isCurrentFollowsMouse() const;
    void setCurrentFollowsMouse(bool follows);
    virtual QRect geometry(int screen) const = 0;
    virtual int number(const QPoint &pos) const = 0;

    inline bool isChanging() { return m_changedTimer->isActive(); }

    int intersecting(const QRect &r) const;

public Q_SLOTS:
    void reconfigure();

Q_SIGNALS:
    void countChanged(int previousCount, int newCount);
    /**
     * Emitted whenever the screens are changed either count or geometry.
     **/
    void changed();

protected Q_SLOTS:
    void setCount(int count);
    void startChangedTimer();
    virtual void updateCount() = 0;

private:
    int m_count;
    int m_current;
    bool m_currentFollowsMouse;
    QTimer *m_changedTimer;
    KSharedConfig::Ptr m_config;

    KWIN_SINGLETON(Screens)
};

class DesktopWidgetScreens : public Screens
{
    Q_OBJECT
public:
    DesktopWidgetScreens(QObject *parent);
    virtual ~DesktopWidgetScreens();
    virtual QRect geometry(int screen) const;
    virtual int number(const QPoint &pos) const;
protected Q_SLOTS:
    void updateCount();

private:
    QDesktopWidget *m_desktop;
};

inline
void Screens::setConfig(KSharedConfig::Ptr config)
{
    m_config = config;
}

inline
int Screens::count() const
{
    return m_count;
}

inline
bool Screens::isCurrentFollowsMouse() const
{
    return m_currentFollowsMouse;
}

inline
void Screens::startChangedTimer()
{
    m_changedTimer->start();
}

inline
Screens *screens()
{
    return Screens::self();
}

}

#endif // KWIN_SCREENS_H
