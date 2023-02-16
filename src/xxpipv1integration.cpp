/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xxpipv1integration.h"
#include "wayland/xxpip_v1.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xxpipv1window.h"

namespace KWin
{

XXPipV1Integration::XXPipV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    XXPipShellV1Interface *shell = new XXPipShellV1Interface(waylandServer()->display(), this);
    connect(shell, &XXPipShellV1Interface::pipCreated,
            this, &XXPipV1Integration::registerPipV1Surface);
}

void XXPipV1Integration::registerPipV1Surface(XXPipV1Interface *pip)
{
    createPipV1Window(pip);
    connect(pip, &XXPipV1Interface::resetOccurred, this, [this, pip] {
        createPipV1Window(pip);
    });
}

void XXPipV1Integration::createPipV1Window(XXPipV1Interface *pip)
{
    if (!workspace()) {
        qCWarning(KWIN_CORE, "An xx-pip-v1 surface has been created while the compositor "
                             "is still not fully initialized. That is a compositor bug!");
        return;
    }

    Q_EMIT windowCreated(new XXPipV1Window(pip));
}

} // namespace KWin
