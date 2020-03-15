/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

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
