/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_IDLE_INTERFACE_H
#define KWAYLAND_SERVER_IDLE_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class IdleInterfacePrivate;

/**
 * @brief Global representing the org_kde_kwin_idle interface.
 *
 * The IdleInterface allows to register callbacks which are invoked if there has
 * not been any user activity (no input) for a specified time span on a seat.
 *
 * A client can bind an idle timeout for a SeatInterface and through that register
 * an idle timeout. The complete interaction is handled internally, thus the API
 * user only needs to create the IdleInterface in order to provide this feature.
 *
 * This interface is useful for clients as it allows them to perform power management,
 * chat applications might want to set to away after no user input for some time, etc.
 *
 * Of course this exposes the global input usage to all clients. Normally clients don't
 * know whether the input devices are used, only if their surfaces have focus. With this
 * interface it is possible to notice that there are input events. A server should consider
 * this to decide whether it wants to provide this feature!
 *
 * @since 5.4
 **/
class KWAYLANDSERVER_EXPORT IdleInterface : public QObject
{
    Q_OBJECT

public:
    explicit IdleInterface(Display *display, QObject *parent = nullptr);
    ~IdleInterface() override;

    /**
     * Inhibits the IdleInterface. While inhibited no IdleTimeoutInterface interface gets
     * notified about an idle timeout.
     *
     * This can be used to inhibit power management, screen locking, etc. directly from
     * Compositor side.
     *
     * To resume idle timeouts invoke @link{uninhibit}. It is possible to invoke inhibit several
     * times, in that case uninhibit needs to called the same amount as inhibit has been called.
     * @see uninhibit
     * @see isInhibited
     * @see inhibitedChanged
     * @since 5.41
     **/
    void inhibit();

    /**
     * Inhibits the IdleInterface. The idle timeouts are only restarted if uninhibit has been
     * called the same amount as inhibit.
     *
     * @see inhibit
     * @see isInhibited
     * @see inhibitedChanged
     * @since 5.41
     **/
    void uninhibit();

    /**
     * @returns Whether idle timeouts are currently inhibited
     * @see inhibit
     * @see uninhibit
     * @see inhibitedChanged
     * @since 5.41
     **/
    bool isInhibited() const;

    /**
     * Calling this method allows the Compositor to simulate user activity.
     * This means the same action is performed as if the user interacted with
     * an input device on the SeatInterface.
     * Idle timeouts are resumed and the idle time gets restarted.
     * @since 5.42
     **/
    void simulateUserActivity();

Q_SIGNALS:
    /**
     * Emitted when the system gets inhibited or uninhibited.
     * @see inhibit
     * @see uninhibit
     * @see isInhibited
     * @since 5.41
     **/
    void inhibitedChanged();

private:
    QScopedPointer<IdleInterfacePrivate> d;
};

}

#endif
