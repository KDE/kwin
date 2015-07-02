/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWAYLAND_SERVER_FAKEINPUT_INTERFACE_H
#define KWAYLAND_SERVER_FAKEINPUT_INTERFACE_H

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"

struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class FakeInputDevice;

class KWAYLANDSERVER_EXPORT FakeInputInterface : public Global
{
    Q_OBJECT
public:
    virtual ~FakeInputInterface();

Q_SIGNALS:
    void deviceCreated(KWayland::Server::FakeInputDevice *device);

private:
    explicit FakeInputInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

class KWAYLANDSERVER_EXPORT FakeInputDevice : public QObject
{
    Q_OBJECT
public:
    virtual ~FakeInputDevice();
    wl_resource *resource();

    void setAuthentication(bool authenticated);
    bool isAuthenticated() const;

Q_SIGNALS:
    void authenticationRequested(const QString &application, const QString &reason);
    void pointerMotionRequested(const QSizeF &delta);
    void pointerButtonPressRequested(quint32 button);
    void pointerButtonReleaseRequested(quint32 button);
    void pointerAxisRequested(Qt::Orientation orientation, qreal delta);

private:
    friend class FakeInputInterface;
    FakeInputDevice(wl_resource *resource, FakeInputInterface *parent);
    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::FakeInputDevice*)

#endif
