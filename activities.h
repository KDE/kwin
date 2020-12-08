/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
class X11Client;

class KWIN_EXPORT Activities : public QObject
{
    Q_OBJECT

public:
    ~Activities() override;

    bool stop(const QString &id);
    bool start(const QString &id);
    void setCurrent(const QString &activity);
    /**
     * Adds/removes client \a c to/from \a activity.
     *
     * Takes care of transients as well.
     */
    void toggleClientOnActivity(X11Client *c, const QString &activity, bool dont_activate);

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
