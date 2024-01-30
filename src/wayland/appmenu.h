/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class Display;
class SurfaceInterface;
class ClientConnection;
class AppMenuInterface;

class AppMenuManagerInterfacePrivate;
class AppMenuInterfacePrivate;
class ClientAppMenuInterface;
class SurfaceAppMenuInterface;

/**
 * Provides the DBus service name and object path to a AppMenu DBus interface.
 *
 * This global can be used for clients to bind AppmenuInterface instances
 * and notifies when a new one is created
 */
class KWIN_EXPORT AppMenuManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit AppMenuManagerInterface(Display *display, QObject *parent = nullptr);
    ~AppMenuManagerInterface() override;
    /**
     * Returns any existing appMenu for a given surface
     * This returns a null pointer if no AppMenuInterface exists.
     */
    SurfaceAppMenuInterface *appMenuForSurface(SurfaceInterface *);

Q_SIGNALS:
    /**
     * Emitted whenever a new AppMenuInterface is created.
     */
    void appMenuCreated(KWin::AppMenuInterface *);

    /**
     * Emitted whenever a new ClientAppMenuInterface is created.
     */
    void clientAppMenuCreated(KWin::ClientAppMenuInterface *);

    /**
     * Emitted whenever a new SurfaceAppMenuInterface is created.
     */
    void surfaceAppMenuCreated(KWin::SurfaceAppMenuInterface *);

private:
    std::unique_ptr<AppMenuManagerInterfacePrivate> d;
};

/**
 * Provides the DBus service name and object path to a AppMenu DBus interface.
 * This interface is attached to a wl_surface or client and provides access to where
 * the AppMenu DBus interface is registered.
 */
class KWIN_EXPORT AppMenuInterface : public QObject
{
    Q_OBJECT
public:
    /**
     * Structure containing DBus service name and path
     */
    struct InterfaceAddress
    {
        /** Service name of host with the AppMenu object*/
        QString serviceName;
        /** Object path of the AppMenu interface*/
        QString objectPath;
    };
    virtual ~AppMenuInterface() override;

    /**
     * @returns the service name and object path or empty strings if unset
     */
    InterfaceAddress address() const;

Q_SIGNALS:
    /**
     * Emitted when the address changes or is first received
     */
    void addressChanged(KWin::AppMenuInterface::InterfaceAddress);

protected:
    // marker function to enforce constructing a subclass
    virtual void _marker() = 0;

private:
    explicit AppMenuInterface(wl_resource *resource);
    friend class AppMenuManagerInterfacePrivate;
    friend class ClientAppMenuInterface;
    friend class SurfaceAppMenuInterface;

    std::unique_ptr<AppMenuInterfacePrivate> d;
};

class KWIN_EXPORT ClientAppMenuInterface final : public AppMenuInterface
{
    Q_OBJECT
public:
    /**
     * @returns The ClientConnection this ClientAppMenuInterface references.
     */
    ClientConnection *client() const;
    ~ClientAppMenuInterface() override;

protected:
    void _marker() override
    {
    }

private:
    explicit ClientAppMenuInterface(ClientConnection *client, wl_resource *resource);
    friend class AppMenuManagerInterfacePrivate;

    ClientConnection *m_client;
};

class KWIN_EXPORT SurfaceAppMenuInterface final : public AppMenuInterface
{
    Q_OBJECT
public:
    /**
     * @returns The SurfaceInterface this SurfaceAppMenuInterface references.
     */
    SurfaceInterface *surface() const;
    ~SurfaceAppMenuInterface() override;

protected:
    void _marker() override
    {
    }

private:
    explicit SurfaceAppMenuInterface(SurfaceInterface *surface, wl_resource *resource);
    friend class AppMenuManagerInterfacePrivate;

    SurfaceInterface *m_surface;
};
}
