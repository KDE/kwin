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
#include "registry.h"

#include <QDebug>

namespace KWin
{
namespace Wayland
{

Registry::Registry(QObject *parent)
    : QObject(parent)
    , m_registry(nullptr)
{
}

Registry::~Registry()
{
    release();
}

void Registry::release()
{
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
}

void Registry::destroy()
{
    if (m_registry) {
        free(m_registry);
        m_registry = nullptr;
    }
}

void Registry::create(wl_display *display)
{
    Q_ASSERT(display);
    Q_ASSERT(!isValid());
    m_registry = wl_display_get_registry(display);
}

void Registry::setup()
{
    Q_ASSERT(isValid());
    wl_registry_add_listener(m_registry, &s_registryListener, this);
}

const struct wl_registry_listener Registry::s_registryListener = {
    Registry::globalAnnounce,
    Registry::globalRemove
};

void Registry::globalAnnounce(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    Registry *r = reinterpret_cast<Registry*>(data);
    Q_ASSERT(registry == r->m_registry);
    r->handleAnnounce(name, interface, version);
}

void Registry::globalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Registry *r = reinterpret_cast<Registry*>(data);
    Q_ASSERT(registry == r->m_registry);
    r->handleRemove(name);
}

static Registry::Interface nameToInterface(const char *interface)
{
    if (strcmp(interface, "wl_compositor") == 0) {
        return Registry::Interface::Compositor;
    } else if (strcmp(interface, "wl_shell") == 0) {
        return Registry::Interface::Shell;
    } else if (strcmp(interface, "wl_seat") == 0) {
        return Registry::Interface::Seat;
    } else if (strcmp(interface, "wl_shm") == 0) {
        return Registry::Interface::Shm;
    } else if (strcmp(interface, "wl_output") == 0) {
        return Registry::Interface::Output;
    }
    return Registry::Interface::Unknown;
}

void Registry::handleAnnounce(uint32_t name, const char *interface, uint32_t version)
{
    Interface i = nameToInterface(interface);
    if (i == Interface::Unknown) {
        qDebug() << "Unknown interface announced: " << interface << "/" << name << "/" << version;
        return;
    }
    qDebug() << "Wayland Interface: " << interface << "/" << name << "/" << version;
    m_interfaces.append({i, name, version});
    switch (i) {
    case Interface::Compositor:
        emit compositorAnnounced(name, version);
        break;
    case Interface::Shell:
        emit shellAnnounced(name, version);
        break;
    case Interface::Output:
        emit outputAnnounced(name, version);
        break;
    case Interface::Seat:
        emit seatAnnounced(name, version);
        break;
    case Interface::Shm:
        emit shmAnnounced(name, version);
        break;
    case Interface::Unknown:
    default:
        // nothing
        break;
    }
}

void Registry::handleRemove(uint32_t name)
{
    auto it = std::find_if(m_interfaces.begin(), m_interfaces.end(),
        [name](const InterfaceData &data) {
            return data.name == name;
        }
    );
    if (it != m_interfaces.end()) {
        InterfaceData data = *(it);
        m_interfaces.erase(it);
        switch (data.interface) {
        case Interface::Compositor:
            emit compositorRemoved(data.name);
            break;
        case Interface::Output:
            emit outputRemoved(data.name);
            break;
        case Interface::Seat:
            emit seatRemoved(data.name);
            break;
        case Interface::Shell:
            emit shellRemoved(data.name);
            break;
        case Interface::Shm:
            emit shmRemoved(data.name);
            break;
        case Interface::Unknown:
        default:
            // nothing
            break;
        }
    }
}

bool Registry::hasInterface(Registry::Interface interface) const
{
    auto it = std::find_if(m_interfaces.begin(), m_interfaces.end(),
        [interface](const InterfaceData &data) {
            return data.interface == interface;
        }
    );
    return it != m_interfaces.end();
}

wl_compositor *Registry::bindCompositor(uint32_t name, uint32_t version) const
{
    return reinterpret_cast<wl_compositor*>(bind(Interface::Compositor, name, version));
}

wl_output *Registry::bindOutput(uint32_t name, uint32_t version) const
{
    return reinterpret_cast<wl_output*>(bind(Interface::Output, name, version));
}

wl_seat *Registry::bindSeat(uint32_t name, uint32_t version) const
{
    return reinterpret_cast<wl_seat*>(bind(Interface::Seat, name, version));
}

wl_shell *Registry::bindShell(uint32_t name, uint32_t version) const
{
    return reinterpret_cast<wl_shell*>(bind(Interface::Shell, name, version));
}

wl_shm *Registry::bindShm(uint32_t name, uint32_t version) const
{
    return reinterpret_cast<wl_shm*>(bind(Interface::Shm, name, version));
}

static const wl_interface *wlInterface(Registry::Interface interface)
{
    switch (interface) {
    case Registry::Interface::Compositor:
        return &wl_compositor_interface;
    case Registry::Interface::Output:
        return &wl_output_interface;
    case Registry::Interface::Seat:
        return &wl_seat_interface;
    case Registry::Interface::Shell:
        return &wl_shell_interface;
    case Registry::Interface::Shm:
        return &wl_shm_interface;
    case Registry::Interface::Unknown:
    default:
        return nullptr;
    }
}

void *Registry::bind(Registry::Interface interface, uint32_t name, uint32_t version) const
{
    auto it = std::find_if(m_interfaces.begin(), m_interfaces.end(), [=](const InterfaceData &data) {
        return data.interface == interface && data.name == name && data.version >= version;
    });
    if (it == m_interfaces.end()) {
        qDebug() << "Don't have interface " << int(interface) << "with name " << name << "and minimum version" << version;
        return nullptr;
    }
    return wl_registry_bind(m_registry, name, wlInterface(interface), version);
}

}
}
