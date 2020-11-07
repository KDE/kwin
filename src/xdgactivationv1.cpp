/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xdgactivationv1.h"
#include "abstract_client.h"
#include "effects.h"
#include "utils.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KWaylandServer/display.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/xdgactivation_v1_interface.h>

using namespace KWaylandServer;

namespace KWin
{
XdgActivationV1Integration::XdgActivationV1Integration(XdgActivationV1Interface *activation, QObject *parent)
    : QObject(parent)
{
    Workspace *ws = Workspace::self();
    connect(ws, &Workspace::clientActivated, this, [this](AbstractClient *client) {
        if (!m_currentActivationToken || !client || client->property("token").toString() == m_currentActivationToken->token) {
            return;
        }
        clear();
    });
    activation->setActivationTokenCreator([this](SurfaceInterface *surface, uint serial, SeatInterface *seat, const QString &appId) -> QString {
        Workspace *ws = Workspace::self();
        if (ws->activeClient()->surface() != surface) {
            qCWarning(KWIN_CORE) << "Inactive surfaces cannot be granted a token";
            return {};
        }

        static int i = 0;
        const auto newToken = QStringLiteral("kwin-%1").arg(++i);

        if (m_currentActivationToken) {
            clear();
        }
        m_currentActivationToken.reset(new ActivationToken{newToken, surface, serial, seat, appId});
        const auto icon = QIcon::fromTheme(AbstractClient::iconFromDesktopFile(appId), QIcon::fromTheme(QStringLiteral("system-run")));
        Q_EMIT effects->startupAdded(m_currentActivationToken->token, icon);

        return newToken;
    });

    connect(activation, &XdgActivationV1Interface::activateRequested, this, &XdgActivationV1Integration::activateSurface);
}

void XdgActivationV1Integration::activateSurface(SurfaceInterface *surface, const QString &token)
{
    Workspace *ws = Workspace::self();
    auto client = waylandServer()->findClient(surface);
    if (!client) {
        qCWarning(KWIN_CORE) << "could not find the toplevel to activate" << surface;
        return;
    }

    if (!m_currentActivationToken || m_currentActivationToken->token != token) {
        qCDebug(KWIN_CORE) << "Refusing to activate " << client << " (provided token: " << token << ", current token:" << (m_currentActivationToken ? m_currentActivationToken->token : QStringLiteral("null")) << ")";
        return;
    }

    auto ownerSurfaceClient = waylandServer()->findClient(m_currentActivationToken->surface);

    qCDebug(KWIN_CORE) << "activating" << client << surface << "on behalf of" << m_currentActivationToken->surface << "into" << ownerSurfaceClient;
    if (ws->activeClient() == ownerSurfaceClient || m_currentActivationToken->applicationId.isEmpty() || ownerSurfaceClient->desktopFileName().isEmpty()) {
        ws->activateClient(client);
    } else {
        qCWarning(KWIN_CORE) << "Activation requested while owner isn't active" << ownerSurfaceClient->desktopFileName()
                                      << m_currentActivationToken->applicationId;
        client->demandAttention();
        clear();
    }
}

void XdgActivationV1Integration::clear()
{
    Q_ASSERT(m_currentActivationToken);
    Q_EMIT effects->startupRemoved(m_currentActivationToken->token);
    m_currentActivationToken.reset();
}

}
