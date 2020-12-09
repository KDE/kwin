/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_FAKEINPUT_INTERFACE_H
#define KWAYLAND_SERVER_FAKEINPUT_INTERFACE_H

#include <QPointF>
#include <QSizeF>
#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class Display;
class FakeInputDevice;
class FakeInputDevicePrivate;
class FakeInputInterfacePrivate;

/**
 * @brief Represents the Global for org_kde_kwin_fake_input interface.
 *
 * The fake input interface allows clients to send fake input events to the
 * Wayland server. For the actual events it creates a FakeInputDevice. Whenever
 * the FakeInputInterface creates a device the signal deviceCreated gets emitted.
 *
 * Accepting fake input events is a security risk. The server should make a
 * dedicated decision about whether it wants to accept fake input events from a
 * device. Because of that by default no events are forwarded to the server. The
 * device needs to request authentication and the server must explicitly authenticate
 * the device. The recommendation is that the server only accepts input for in some
 * way trusted clients.
 *
 * @see FakeInputDevice
 * @since 5.4
 **/
class KWAYLANDSERVER_EXPORT FakeInputInterface : public QObject
{
    Q_OBJECT

public:
    explicit FakeInputInterface(Display *display, QObject *parent = nullptr);
    ~FakeInputInterface() override;

Q_SIGNALS:
    /**
     * Signal emitted whenever a client bound the fake input @p device.
     * @param device The created FakeInputDevice
     **/
    void deviceCreated(KWaylandServer::FakeInputDevice *device);

private:
    QScopedPointer<FakeInputInterfacePrivate> d;
};

/**
 * @brief Represents the Resource for a org_kde_kwin_fake_input interface.
 *
 * @see FakeInputInterface
 * @since 5.4
 **/
class KWAYLANDSERVER_EXPORT FakeInputDevice : public QObject
{
    Q_OBJECT
public:
    ~FakeInputDevice() override;
    /**
     * @returns the native wl_resource.
     **/
    wl_resource *resource();

    /**
     * Authenticate this device to send events. If @p authenticated is @c true events are
     * accepted, for @c false events are no longer accepted.
     *
     * @param authenticated Whether the FakeInputDevice should be considered authenticated
     **/
    void setAuthentication(bool authenticated);
    /**
     * @returns whether the FakeInputDevice is authenticated and allowed to send events, default is @c false.
     **/
    bool isAuthenticated() const;

Q_SIGNALS:
    /**
     * Request for authentication.
     *
     * The server might use the provided information to make a decision on whether the
     * FakeInputDevice should get authenticated. It is recommended to not trust the data
     * and to combine it with information from ClientConnection.
     *
     * @param application A textual description of the application
     * @param reason A textual description of the reason why the application wants to send fake input events
     **/
    void authenticationRequested(const QString &application, const QString &reason);
    /**
     * Request a pointer motion by @p delta.
     **/
    void pointerMotionRequested(const QSizeF &delta);
    /**
     * Request an absolute pointer motion to @p pos.
     *
     * @since 5.54
     **/
    void pointerMotionAbsoluteRequested(const QPointF &pos);
    /**
     * Requests a pointer button pressed for @p button.
     **/
    void pointerButtonPressRequested(quint32 button);
    /**
     * Requests a pointer button release for @p button.
     **/
    void pointerButtonReleaseRequested(quint32 button);
    /**
     * Requests a pointer axis for the given @p orientation by @p delta.
     **/
    void pointerAxisRequested(Qt::Orientation orientation, qreal delta);
    /**
     * Requests a touch down at @p pos and identified by @p id.
     *
     * @since 5.23
     **/
    void touchDownRequested(quint32 id, const QPointF &pos);
    /**
     * Requests a touch motion by @p pos and identified by @p id.
     *
     * @since 5.23
     **/
    void touchMotionRequested(quint32 id, const QPointF &pos);
    /**
     * Requests a touch up identified by @p id.
     *
     * @since 5.23
     **/
    void touchUpRequested(quint32 id);
    /**
     * Requests a touch cancel event.
     *
     * @since 5.23
     **/
    void touchCancelRequested();
    /**
     * Requests a touch frame event.
     *
     * @since 5.23
     **/
    void touchFrameRequested();
     /**
     * Requests a keyboard key pressed for @p key.
     *
     * @since 5.63
     **/
    void keyboardKeyPressRequested(quint32 key);
    /**
     * Requests a keyboard key release for @p key.
     *
     * @since 5.63
     **/
    void keyboardKeyReleaseRequested(quint32 key);

private:
    friend class FakeInputInterfacePrivate;
    FakeInputDevice(FakeInputInterface *parent, wl_resource *resource);
    QScopedPointer<FakeInputDevicePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::FakeInputDevice*)

#endif
