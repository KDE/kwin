/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QStringList>
#include <kwin_export.h>
#include <unordered_map>

#include <KSharedConfig>
#include <PlasmaActivities/Controller>

namespace KActivities
{
class Controller;
}

namespace KWin
{
class Window;
class VirtualDesktop;

class KWIN_EXPORT Activities : public QObject
{
    Q_OBJECT

public:
    explicit Activities(const KSharedConfig::Ptr &config);

    /**
     * Sets the current activity to @param activity, and if desktop isn't nullptr,
     * ensures that this doesn't interfere with virtual desktop switching
     */
    void setCurrent(const QString &activity, VirtualDesktop *desktop);
    /**
     * Adds/removes window \a window to/from \a activity.
     *
     * Takes care of transients as well.
     */
    void toggleWindowOnActivity(Window *window, const QString &activity, bool dont_activate);

    QStringList running() const;
    QStringList all() const;
    const QString &current() const;
    const QString &previous() const;

    static QString nullUuid();

    KActivities::Controller::ServiceStatus serviceStatus() const;

Q_SIGNALS:
    /**
     * emitted before the current activity actually changes
     */
    void currentAboutToChange();
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

public Q_SLOTS:
    void notifyCurrentDesktopChanged(VirtualDesktop *desktop);

private Q_SLOTS:
    void slotServiceStatusChanged();
    void slotRemoved(const QString &activity);
    void slotCurrentChanged(const QString &newActivity);

private:
    QString m_previous;
    QString m_current;
    KActivities::Controller *m_controller;
    std::unordered_map<QString, QString> m_lastVirtualDesktop;
    KSharedConfig::Ptr m_config;
};

inline QStringList Activities::all() const
{
    return m_controller->activities();
}

inline const QString &Activities::current() const
{
    return m_current;
}

inline const QString &Activities::previous() const
{
    return m_previous;
}

inline QStringList Activities::running() const
{
    return m_controller->activities(KActivities::Info::Running);
}

inline QString Activities::nullUuid()
{
    // cloned from kactivities/src/lib/core/consumer.cpp
    return QStringLiteral("00000000-0000-0000-0000-000000000000");
}

}
