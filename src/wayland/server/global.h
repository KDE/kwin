/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_GLOBAL_H
#define WAYLAND_SERVER_GLOBAL_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_global;

namespace KWaylandServer
{
class Display;

/**
 * @brief Base class for all Globals.
 *
 * Any class representing a Global should be derived from this base class.
 * This class provides common functionality for all globals. A global is an
 * object listed as an interface on the registry on client side.
 *
 * Normally a Global gets factored by the Display. For each Global-derived class there
 * is a dedicated factory method. After creating an instance through the factory method
 * it is not yet announced on the registry. One needs to call ::create on it. This allows
 * to setup the Global before it gets announced, ensuring that the client's state is correct
 * from the start.
 *
 * As an example shown for @link OutputInterface @endlink:
 * @code
 * Display *display; // The existing display
 * auto o = display->createOutput();
 * o->setManufacturer(QStringLiteral("The KDE Community"));
 * // setup further data on the OutputInterface
 * o->create(); // announces OutputInterface
 * @endcode
 *
 * @see Display
 *
 **/
class KWAYLANDSERVER_EXPORT Global : public QObject
{
    Q_OBJECT
public:
    virtual ~Global();
    /**
     * Creates the global by creating a native wl_global and by that announcing it
     * to the clients.
     **/
    void create();
    /**
     * Destroys the low level wl_global. Afterwards the Global is no longer shown to clients.
     **/
    void destroy();
    /**
     * @returns whether the Global got created
     **/
    bool isValid() const;

    /**
     * @returns the Display the Global got created on.
     */
    Display *display();

    /**
     * Cast operator to the native wl_global this Global represents.
     **/
    operator wl_global*();
    /**
     * Cast operator to the native wl_global this Global represents.
     **/
    operator wl_global*() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the client is in the process of removing the wl_global.
     * At the time the signal is emitted the global is still valid and allows to perform
     * cleanup tasks.
     */
    void aboutToDestroyGlobal();

protected:
    class Private;
    explicit Global(Private *d, QObject *parent = nullptr);
    QScopedPointer<Private> d;
};

}

#endif
