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
#ifndef KWIN_WAYLAND_REGISTRY_H
#define KWIN_WAYLAND_REGISTRY_H

#include <QHash>
#include <QObject>

#include <wayland-client-protocol.h>

namespace KWin
{
namespace Wayland
{

class Registry : public QObject
{
    Q_OBJECT
public:
    enum class Interface {
        Compositor, // wl_compositor
        Shell,      // wl_shell
        Seat,       // wl_seat
        Shm,        // wl_shm
        Output,     // wl_output
        Unknown
    };
    explicit Registry(QObject *parent = nullptr);
    virtual ~Registry();

    void release();
    void destroy();
    void create(wl_display *display);
    void setup();

    bool isValid() const {
        return m_registry != nullptr;
    }
    bool hasInterface(Interface interface) const;

    wl_compositor *bindCompositor(uint32_t name, uint32_t version) const;
    wl_shell *bindShell(uint32_t name, uint32_t version) const;
    wl_seat *bindSeat(uint32_t name, uint32_t version) const;
    wl_shm *bindShm(uint32_t name, uint32_t version) const;
    wl_output *bindOutput(uint32_t name, uint32_t version) const;

    operator wl_registry*() {
        return m_registry;
    }
    operator wl_registry*() const {
        return m_registry;
    }
    wl_registry *registry() {
        return m_registry;
    }

    static void globalAnnounce(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void globalRemove(void *data, struct wl_registry *registry, uint32_t name);

Q_SIGNALS:
    void compositorAnnounced(quint32 name, quint32 version);
    void shellAnnounced(quint32 name, quint32 version);
    void seatAnnounced(quint32 name, quint32 version);
    void shmAnnounced(quint32 name, quint32 version);
    void outputAnnounced(quint32 name, quint32 version);
    void compositorRemoved(quint32 name);
    void shellRemoved(quint32 name);
    void seatRemoved(quint32 name);
    void shmRemoved(quint32 name);
    void outputRemoved(quint32 name);

private:
    static const struct wl_registry_listener s_registryListener;
    void handleAnnounce(uint32_t name, const char *interface, uint32_t version);
    void handleRemove(uint32_t name);
    void *bind(Interface interface, uint32_t name, uint32_t version) const;

    wl_registry *m_registry;
    struct InterfaceData {
        Interface interface;
        uint32_t name;
        uint32_t version;
    };
    QList<InterfaceData> m_interfaces;
};

}
}

#endif
