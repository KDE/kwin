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
#ifndef WAYLAND_SERVER_SEAT_INTERFACE_H
#define WAYLAND_SERVER_SEAT_INTERFACE_H

#include <QObject>
#include <QPoint>

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"
#include "pointer_interface.h"

struct wl_client;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class KeyboardInterface;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT SeatInterface : public Global
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool pointer READ hasPointer WRITE setHasPointer NOTIFY hasPointerChanged)
    Q_PROPERTY(bool keyboard READ hasKeyboard WRITE setHasKeyboard NOTIFY hasKeyboardChanged)
    Q_PROPERTY(bool tourch READ hasTouch WRITE setHasTouch NOTIFY hasTouchChanged)
    Q_PROPERTY(QPointF pointerPos READ pointerPos WRITE setPointerPos NOTIFY pointerPosChanged)
    Q_PROPERTY(quint32 timestamp READ timestamp WRITE setTimestamp NOTIFY timestampChanged)
public:
    virtual ~SeatInterface();

    QString name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;
    KeyboardInterface *keyboard();

    void setName(const QString &name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    void setTimestamp(quint32 time);
    quint32 timestamp() const;

    // pointer related methods
    void setPointerPos(const QPointF &pos);
    QPointF pointerPos() const;
    void setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition = QPoint());
    SurfaceInterface *focusedPointerSurface() const;
    PointerInterface *focusedPointer() const;
    void setFocusedPointerSurfacePosition(const QPointF &surfacePosition);
    QPointF focusedPointerSurfacePosition() const;

    static SeatInterface *get(wl_resource *native);

Q_SIGNALS:
    void nameChanged(const QString&);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);
    void pointerPosChanged(const QPointF &pos);
    void timestampChanged(quint32);

    void pointerCreated(KWayland::Server::PointerInterface*);

private:
    friend class Display;
    explicit SeatInterface(Display *display, QObject *parent);

    class Private;
    Private *d_func() const;
};

}
}

#endif
