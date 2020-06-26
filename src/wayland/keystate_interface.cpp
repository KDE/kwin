/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "keystate_interface.h"
#include "display.h"

#include <QDebug>
#include <QVector>
#include <qwayland-server-keystate.h>

namespace KWaylandServer
{

static const quint32 s_version = 1;

class KeyStateInterfacePrivate : public QtWaylandServer::org_kde_kwin_keystate
{
public:
    KeyStateInterfacePrivate(Display *d)
        : QtWaylandServer::org_kde_kwin_keystate(*d, s_version)
    {}

    void org_kde_kwin_keystate_fetchStates(Resource *resource) override {
        for (int i = 0; i < m_keyStates.count(); ++i) {
            send_stateChanged(resource->handle, i, m_keyStates[i]);
        }
    }

    QVector<KeyStateInterface::State> m_keyStates = QVector<KeyStateInterface::State>(3, KeyStateInterface::Unlocked);
};

KeyStateInterface::KeyStateInterface(Display* d, QObject* parent)
    : QObject(parent)
    , d(new KeyStateInterfacePrivate(d))
{}

KeyStateInterface::~KeyStateInterface() = default;

void KeyStateInterface::setState(KeyStateInterface::Key key, KeyStateInterface::State state)
{
    d->m_keyStates[int(key)] = state;

    for (auto r : d->resourceMap()) {
        d->send_stateChanged(r->handle, int(key), int(state));
    }
}

}
