/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fullscreenshellv1client.h"
#include "abstract_wayland_output.h"
#include "deleted.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KWaylandServer/surface_interface.h>

namespace KWin
{
using namespace KWaylandServer;

FullscreenShellV1Client::FullscreenShellV1Client(SurfaceInterface *surfaceIface)
    : WaylandClient(surfaceIface)
{
    connect(surface(), &SurfaceInterface::aboutToBeDestroyed, this, &FullscreenShellV1Client::destroyClient);
    connect(surface(), &SurfaceInterface::sizeChanged, this, &FullscreenShellV1Client::reposition);

    if (surface()->isMapped()) {
        updateDepth();
        QTimer::singleShot(0, this, &FullscreenShellV1Client::setReadyForPainting);
    } else {
        connect(surface(), &SurfaceInterface::mapped, this, &FullscreenShellV1Client::updateDepth);
        connect(surface(), &SurfaceInterface::mapped, this, &FullscreenShellV1Client::setReadyForPainting);
    }
}

void FullscreenShellV1Client::destroyClient()
{
    markAsZombie();
    if (isMoveResize()) {
        leaveMoveResize();
    }
    cleanTabBox();
    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    RuleBook::self()->discardUsed(this, true);
    destroyDecoration();
    cleanGrouping();
    waylandServer()->removeClient(this);
    deleted->unrefWindow();
    delete this;
}

void FullscreenShellV1Client::reposition()
{
    QSize clientSize = surface()->size();
    if (!clientSize.isValid() || clientSize.isEmpty()) {
        return;
    }

    const QRect availableArea = workspace()->clientArea(FullScreenArea, m_output, desktop());

    //NOTE: this is where we'd take the PresentMethod m_method into account, ignoring it for now
    clientSize = availableArea.size();

    QRect geo(availableArea.topLeft(), clientSize);
    geo.translate((availableArea.width() - clientSize.width()) / 2, (availableArea.height() - clientSize.height()) / 2);
    updateGeometry(geo);
}

NET::WindowType FullscreenShellV1Client::windowType(bool /*direct*/, int /*supportedTypes*/) const
{
    return NET::Normal;
}

bool FullscreenShellV1Client::takeFocus()
{
    setActive(true);
    if (!keepAbove() && !belongsToDesktop()) {
        workspace()->setShowingDesktop(false);
    }
    return true;
}

void FullscreenShellV1Client::setPresentMethod(KWaylandServer::FullscreenShellV1Interface::PresentMethod method)
{
    if (m_method == method) {
        return;
    }

    m_method = method;
    reposition();
}

void FullscreenShellV1Client::setOutput(KWaylandServer::OutputInterface *outputIface)
{
    AbstractWaylandOutput *output = waylandServer()->findOutput(outputIface);
    if (m_output == output) {
        return;
    }

    m_output = output;
    reposition();
}

}
