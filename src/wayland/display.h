/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_DISPLAY_H
#define WAYLAND_SERVER_DISPLAY_H

#include <QList>
#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "clientconnection.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;

namespace KWayland
{
namespace Server
{

class CompositorInterface;
class DataDeviceManagerInterface;
class OutputInterface;
class SeatInterface;
class ShellInterface;
class SubCompositorInterface;

class KWAYLANDSERVER_EXPORT Display : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString socketName READ socketName WRITE setSocketName NOTIFY socketNameChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit Display(QObject *parent = nullptr);
    virtual ~Display();

    void setSocketName(const QString &name);
    QString socketName() const;

    quint32 serial();
    quint32 nextSerial();

    void start();
    void terminate();
    /**
     * Starts the event loop for the server socket.
     * This method should only be used if start() is used before creating the
     * QCoreApplication. In that case start() cannot fully setup the event processing
     * and the loop needs to be started after the QCoreApplication got created.
     * @see start
     * @see dispatchEvents
     **/
    void startLoop();
    /**
     * Dispatches pending events in a blocking way. May only be used if the Display is
     * created and started before the QCoreApplication is created. Once the QCoreApplication
     * is created and the event loop is started this method may no longer be invoked.
     * @see startLoop
     **/
    void dispatchEvents(int msecTimeout = -1);

    operator wl_display*();
    operator wl_display*() const;
    bool isRunning() const;

    OutputInterface *createOutput(QObject *parent = nullptr);
    void removeOutput(OutputInterface *output);
    QList<OutputInterface*> outputs() const;

    CompositorInterface *createCompositor(QObject *parent = nullptr);
    void createShm();
    ShellInterface *createShell(QObject *parent = nullptr);
    SeatInterface *createSeat(QObject *parent = nullptr);
    SubCompositorInterface *createSubCompositor(QObject *parent = nullptr);
    DataDeviceManagerInterface *createDataDeviceManager(QObject *parent = nullptr);

    /**
     * Gets the ClientConnection for the given @p client.
     * If there is no ClientConnection yet for the given @p client, it will be created.
     * @param client The native client for which the ClientConnection is retrieved
     * @return The ClientConnection for the given native client
     **/
    ClientConnection *getConnection(wl_client *client);

Q_SIGNALS:
    void socketNameChanged(const QString&);
    void runningChanged(bool);
    void aboutToTerminate();
    void clientConnected(KWayland::Server::ClientConnection*);
    void clientDisconnected(KWayland::Server::ClientConnection*);

private:
    class Private;
    QScopedPointer<Private> d;
};

}
}

#endif
