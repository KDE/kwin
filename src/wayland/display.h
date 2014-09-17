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
#ifndef KWIN_WAYLAND_SERVER_DISPLAY_H
#define KWIN_WAYLAND_SERVER_DISPLAY_H

#include <QList>
#include <QObject>

#include <kwaylandserver_export.h>

struct wl_display;
struct wl_event_loop;

namespace KWayland
{
namespace Server
{

class CompositorInterface;
class OutputInterface;
class SeatInterface;
class ShellInterface;

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

    operator wl_display*() {
        return m_display;
    }
    operator wl_display*() const {
        return m_display;
    }
    bool isRunning() const {
        return m_running;
    }

    OutputInterface *createOutput(QObject *parent = nullptr);
    void removeOutput(OutputInterface *output);
    const QList<OutputInterface*> &outputs() const {
        return m_outputs;
    }

    CompositorInterface *createCompositor(QObject *parent = nullptr);
    void createShm();
    ShellInterface *createShell(QObject *parent = nullptr);
    SeatInterface *createSeat(QObject *parent = nullptr);

Q_SIGNALS:
    void socketNameChanged(const QString&);
    void runningChanged(bool);
    void aboutToTerminate();

private:
    void flush();
    void setRunning(bool running);
    wl_display *m_display;
    wl_event_loop *m_loop;
    QString m_socketName;
    bool m_running;
    QList<OutputInterface*> m_outputs;
};

}
}

#endif
