/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fullscreenshellv1integration.h"
#include "fullscreenshellv1client.h"
#include "wayland_server.h"
#include <KWaylandServer/fullscreenshell_v1_interface.h>
#include <KWaylandServer/surface_interface.h>

namespace KWin
{
FullscreenShellV1Integration::FullscreenShellV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    using KWaylandServer::FullscreenShellV1Interface;
    const auto caps = FullscreenShellV1Interface::CursorPlane;
    auto shell = new FullscreenShellV1Interface(caps, waylandServer()->display(), this);
    connect(shell,
            &FullscreenShellV1Interface::presentSurface,
            this,
            [this](FullscreenShellV1Interface::PresentMethod method, KWaylandServer::SurfaceInterface *surface, KWaylandServer::OutputInterface *output) {
                auto c = waylandServer()->findClient(surface);
                auto shellClient = qobject_cast<FullscreenShellV1Client *>(c);
                if (c && !shellClient) {
                    qCWarning(KWIN_CORE) << "Trying to display a surface already represented by a client" << c;
                    return;
                }

                if (!shellClient) {
                    shellClient = new FullscreenShellV1Client(surface);
                    shellClient->setPresentMethod(method);
                    shellClient->setOutput(output);
                    Q_EMIT clientCreated(shellClient);
                } else {
                    shellClient->setPresentMethod(method);
                    shellClient->setOutput(output);
                }
            });
}

}
