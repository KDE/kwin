/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_WAYLAND_SEAT_H
#define KWIN_WAYLAND_SEAT_H

#include <QObject>
#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class Keyboard;
class Pointer;

class Seat : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool keyboard READ hasKeyboard NOTIFY hasKeyboardChanged)
    Q_PROPERTY(bool pointer READ hasPointer NOTIFY hasPointerChanged)
    Q_PROPERTY(bool touch READ hasTouch NOTIFY hasTouchChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
public:
    explicit Seat(QObject *parent = nullptr);
    virtual ~Seat();

    bool isValid() const {
        return m_seat != nullptr;
    }
    void setup(wl_seat *seat);
    void release();
    void destroy();

    bool hasKeyboard() const {
        return m_capabilityKeyboard;
    }
    bool hasPointer() const {
        return m_capabilityPointer;
    }
    bool hasTouch() const {
        return m_capabilityTouch;
    }
    const QString &name() const {
        return m_name;
    }
    operator wl_seat*() {
        return m_seat;
    }
    operator wl_seat*() const {
        return m_seat;
    }

    Keyboard *createKeyboard(QObject *parent = nullptr);
    Pointer *createPointer(QObject *parent = nullptr);
    wl_touch *createTouch();

    static void capabilitiesCallback(void *data, wl_seat *seat, uint32_t capabilities);
    static void nameCallback(void *data, wl_seat *wl_seat, const char *name);

Q_SIGNALS:
    void hasKeyboardChanged(bool);
    void hasPointerChanged(bool);
    void hasTouchChanged(bool);
    void nameChanged(const QString &name);

private:
    void resetSeat();
    void setHasKeyboard(bool has);
    void setHasPointer(bool has);
    void setHasTouch(bool has);
    void capabilitiesChanged(uint32_t capabilities);
    void setName(const QString &name);

    wl_seat *m_seat;
    bool m_capabilityKeyboard;
    bool m_capabilityPointer;
    bool m_capabilityTouch;
    QString m_name;
    static const wl_seat_listener s_listener;
};

}
}

#endif
