/*
    SPDX-FileCopyrightText: 2017 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class SurfaceInterface;
class AppMenuInterface;

class AppMenuManagerInterfacePrivate;
class AppMenuInterfacePrivate;

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
    AppMenuInterface *appMenuForSurface(SurfaceInterface *);

Q_SIGNALS:
    /**
     * Emitted whenever a new AppmenuInterface is created.
     */
    void appMenuCreated(KWaylandServer::AppMenuInterface *);

private:
    std::unique_ptr<AppMenuManagerInterfacePrivate> d;
};

/**
 * Provides the DBus service name and object path to a AppMenu DBus interface.
 * This interface is attached to a wl_surface and provides access to where
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
    ~AppMenuInterface() override;

    /**
     * @returns the service name and object path or empty strings if unset
     */
    InterfaceAddress address() const;

    /**
     * @returns The SurfaceInterface this AppmenuInterface references.
     */
    SurfaceInterface *surface() const;

Q_SIGNALS:
    /**
     * Emitted when the address changes or is first received
     */
    void addressChanged(KWaylandServer::AppMenuInterface::InterfaceAddress);

private:
    explicit AppMenuInterface(SurfaceInterface *s, wl_resource *resource);
    friend class AppMenuManagerInterfacePrivate;

    std::unique_ptr<AppMenuInterfacePrivate> d;
};

}
