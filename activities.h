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
#ifndef KWIN_ACTIVITIES_H
#define KWIN_ACTIVITIES_H

#include <kwinglobals.h>

#include <QObject>
#include <QStringList>

#include <kactivities/controller.h>

namespace KActivities {
class Controller;
}

namespace KWin
{
class Client;

class KWIN_EXPORT Activities : public QObject
{
    Q_OBJECT

public:
    ~Activities();

    bool stop(const QString &id);
    bool start(const QString &id);
    void setCurrent(const QString &activity);
    /**
    * Adds/removes client \a c to/from \a activity.
    *
    * Takes care of transients as well.
    */
    void toggleClientOnActivity(Client* c, const QString &activity, bool dont_activate);

    QStringList running() const;
    QStringList all() const;
    const QString &current() const;
    const QString &previous() const;

    static QString nullUuid();

    KActivities::Controller::ServiceStatus serviceStatus() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the global
     * activity is changed
     * @param id id of the new current activity
     */
    void currentChanged(const QString &id);
    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     */
    void added(const QString &id);
    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     */
    void removed(const QString &id);

private Q_SLOTS:
    void slotRemoved(const QString &activity);
    void slotCurrentChanged(const QString &newActivity);
    void reallyStop(const QString &id);   //dbus deadlocks suck

private:
    QString m_previous;
    QString m_current;
    KActivities::Controller *m_controller;

    KWIN_SINGLETON(Activities)
};

inline
QStringList Activities::all() const
{
    return m_controller->activities();
}

inline
const QString &Activities::current() const
{
    return m_current;
}

inline
const QString &Activities::previous() const
{
    return m_previous;
}

inline
QStringList Activities::running() const
{
    return m_controller->activities(KActivities::Info::Running);
}

inline
QString Activities::nullUuid()
{
    // cloned from kactivities/src/lib/core/consumer.cpp
    return QStringLiteral("00000000-0000-0000-0000-000000000000");
}

}

#endif // KWIN_ACTIVITIES_H
