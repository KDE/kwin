/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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

#include "keystate_interface.h"
#include "global_p.h"
#include "display.h"

#include <QDebug>
#include <QVector>
#include <wayland-server.h>
#include <wayland-keystate-server-protocol.h>

namespace KWayland
{
namespace Server
{

class KeyStateInterface::Private : public Global::Private
{
public:
    Private(Display *d)
        : Global::Private(d, &org_kde_kwin_keystate_interface, s_version)
    {}

    void bind(wl_client * client, uint32_t version, uint32_t id) override {
        auto c = display->getConnection(client);
        wl_resource *resource = c->createResource(&org_kde_kwin_keystate_interface, qMin(version, s_version), id);
        if (!resource) {
            wl_client_post_no_memory(client);
            return;
        }
        wl_resource_set_implementation(resource, &s_interface, this, unbind);
        m_resources << resource;
    }

    static void unbind(wl_resource *resource) {
        auto *d = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
        d->m_resources.removeAll(resource);
    }


    static void fetchStatesCallback(struct wl_client */*client*/, struct wl_resource *resource) {
        auto s = reinterpret_cast<KeyStateInterface::Private*>(wl_resource_get_user_data(resource));

        for (int i=0; i<s->m_keyStates.count(); ++i)
            org_kde_kwin_keystate_send_stateChanged(resource, i, s->m_keyStates[i]);
    }

    static const quint32 s_version;
    QVector<wl_resource*> m_resources;
    QVector<State> m_keyStates = QVector<State>(3, Unlocked);
    static const struct org_kde_kwin_keystate_interface s_interface;
};

const quint32 KeyStateInterface::Private::s_version = 1;

KeyStateInterface::KeyStateInterface(Display* d, QObject* parent)
    : Global(new Private(d), parent)
{}

KeyStateInterface::~KeyStateInterface() = default;

const struct org_kde_kwin_keystate_interface KeyStateInterface::Private::s_interface = {
    fetchStatesCallback
};

void KeyStateInterface::setState(KeyStateInterface::Key key, KeyStateInterface::State state)
{
    auto dptr = static_cast<KeyStateInterface::Private*>(d.data());
    dptr->m_keyStates[int(key)] = state;

    for(auto r : qAsConst(dptr->m_resources)) {
        org_kde_kwin_keystate_send_stateChanged(r, int(key), int(state));
    }
}

}

}
