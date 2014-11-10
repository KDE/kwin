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

struct wl_client;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;
class KeyboardInterface;
class PointerInterface;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT SeatInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool pointer READ hasPointer WRITE setHasPointer NOTIFY hasPointerChanged)
    Q_PROPERTY(bool keyboard READ hasKeyboard WRITE setHasKeyboard NOTIFY hasKeyboardChanged)
    Q_PROPERTY(bool tourch READ hasTouch WRITE setHasTouch NOTIFY hasTouchChanged)
public:
    virtual ~SeatInterface();

    void create();
    void destroy();
    bool isValid() const;

    QString name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;
    PointerInterface *pointer();
    KeyboardInterface *keyboard();

    void setName(const QString &name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    static SeatInterface *get(wl_resource *native);

Q_SIGNALS:
    void nameChanged(const QString&);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);

private:
    friend class Display;
    explicit SeatInterface(Display *display, QObject *parent);

    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT PointerInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPoint globalPos READ globalPos WRITE setGlobalPos NOTIFY globalPosChanged)
public:
    virtual ~PointerInterface();

    void createInterface(wl_client *client, wl_resource *parentResource, uint32_t id);

    void updateTimestamp(quint32 time);
    void setGlobalPos(const QPoint &pos);
    QPoint globalPos() const;
    void buttonPressed(quint32 button);
    void buttonPressed(Qt::MouseButton button);
    void buttonReleased(quint32 button);
    void buttonReleased(Qt::MouseButton button);
    bool isButtonPressed(quint32 button) const;
    bool isButtonPressed(Qt::MouseButton button) const;
    quint32 buttonSerial(quint32 button) const;
    quint32 buttonSerial(Qt::MouseButton button) const;
    void axis(Qt::Orientation orientation, quint32 delta);

    void setFocusedSurface(SurfaceInterface *surface, const QPoint &surfacePosition = QPoint());
    void setFocusedSurfacePosition(const QPoint &surfacePosition);
    SurfaceInterface *focusedSurface() const;
    QPoint focusedSurfacePosition() const;

Q_SIGNALS:
    void globalPosChanged(const QPoint &pos);

private:
    friend class SeatInterface;
    explicit PointerInterface(Display *display, SeatInterface *parent);
    class Private;
    QScopedPointer<Private> d;
};

class KWAYLANDSERVER_EXPORT KeyboardInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~KeyboardInterface();

    void createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id);

    void updateTimestamp(quint32 time);
    void setKeymap(int fd, quint32 size);
    void keyPressed(quint32 key);
    void keyReleased(quint32 key);
    void updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);

    void setFocusedSurface(SurfaceInterface *surface);
    SurfaceInterface *focusedSurface() const;

private:
    friend class SeatInterface;
    explicit KeyboardInterface(Display *display, SeatInterface *parent);

    class Private;
    QScopedPointer<Private> d;
};

}
}

#endif
