/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layershellv1integration.h"
#include "abstract_wayland_output.h"
#include "layershellv1client.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/layershell_v1_interface.h>

#include <QTimer>

using namespace KWaylandServer;

namespace KWin
{

static const Qt::Edges AnchorHorizontal = Qt::LeftEdge | Qt::RightEdge;
static const Qt::Edges AnchorVertical = Qt::TopEdge | Qt::BottomEdge;

LayerShellV1Integration::LayerShellV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    LayerShellV1Interface *shell = waylandServer()->display()->createLayerShellV1(this);
    connect(shell, &KWaylandServer::LayerShellV1Interface::surfaceCreated,
            this, &LayerShellV1Integration::createClient);

    m_rearrangeTimer = new QTimer(this);
    m_rearrangeTimer->setSingleShot(true);
    connect(m_rearrangeTimer, &QTimer::timeout, this, &LayerShellV1Integration::rearrange);
}

void LayerShellV1Integration::createClient(LayerSurfaceV1Interface *shellSurface)
{
    AbstractOutput *output = waylandServer()->findOutput(shellSurface->output());
    if (!output) {
        output = kwinApp()->platform()->findOutput(screens()->current());
    }
    if (!output) {
        qCWarning(KWIN_CORE) << "Could not find any suitable output for a layer surface";
        shellSurface->sendClosed();
        return;
    }

    emit clientCreated(new LayerShellV1Client(shellSurface, output, this));
}

void LayerShellV1Integration::recreateClient(LayerSurfaceV1Interface *shellSurface)
{
    destroyClient(shellSurface);
    createClient(shellSurface);
}

void LayerShellV1Integration::destroyClient(LayerSurfaceV1Interface *shellSurface)
{
    const QList<AbstractClient *> clients = waylandServer()->clients();
    for (AbstractClient *client : clients) {
        LayerShellV1Client *layerShellClient = qobject_cast<LayerShellV1Client *>(client);
        if (layerShellClient && layerShellClient->shellSurface() == shellSurface) {
            layerShellClient->destroyClient();
            break;
        }
    }
}

static void adjustWorkArea(const LayerSurfaceV1Interface *shellSurface, QRect *workArea)
{
    switch (shellSurface->exclusiveEdge()) {
    case Qt::LeftEdge:
        workArea->adjust(shellSurface->leftMargin() + shellSurface->exclusiveZone(), 0, 0, 0);
        break;
    case Qt::RightEdge:
        workArea->adjust(0, 0, -shellSurface->rightMargin() - shellSurface->exclusiveZone(), 0);
        break;
    case Qt::TopEdge:
        workArea->adjust(0, shellSurface->topMargin() + shellSurface->exclusiveZone(), 0, 0);
        break;
    case Qt::BottomEdge:
        workArea->adjust(0, 0, 0, -shellSurface->bottomMargin() - shellSurface->exclusiveZone());
        break;
    }
}

static void rearrangeLayer(const QList<LayerShellV1Client *> &clients, QRect *workArea,
                           LayerSurfaceV1Interface::Layer layer, bool exclusive)
{
    for (LayerShellV1Client *client : clients) {
        LayerSurfaceV1Interface *shellSurface = client->shellSurface();

        if (shellSurface->layer() != layer) {
            continue;
        }
        if (exclusive != (shellSurface->exclusiveZone() > 0)) {
            continue;
        }

        QRect bounds;
        if (shellSurface->exclusiveZone() == -1) {
            bounds = workspace()->clientArea(ScreenArea, client);
        } else {
            bounds = *workArea;
        }

        QRect geometry(QPoint(0, 0), shellSurface->desiredSize());

        if ((shellSurface->anchor() & AnchorHorizontal) && geometry.width() == 0) {
            geometry.setLeft(bounds.left());
            geometry.setWidth(bounds.width());
        } else if (shellSurface->anchor() & Qt::LeftEdge) {
            geometry.moveLeft(bounds.left());
        } else if (shellSurface->anchor() & Qt::RightEdge) {
            geometry.moveRight(bounds.right());
        } else {
            geometry.moveLeft(bounds.left() + (bounds.width() - geometry.width()) / 2);
        }

        if ((shellSurface->anchor() & AnchorVertical) && geometry.height() == 0) {
            geometry.setTop(bounds.top());
            geometry.setHeight(bounds.height());
        } else if (shellSurface->anchor() & Qt::TopEdge) {
            geometry.moveTop(bounds.top());
        } else if (shellSurface->anchor() & Qt::BottomEdge) {
            geometry.moveBottom(bounds.bottom());
        } else {
            geometry.moveTop(bounds.top() + (bounds.height() - geometry.height()) / 2);
        }

        if ((shellSurface->anchor() & AnchorHorizontal) == AnchorHorizontal) {
            geometry.adjust(shellSurface->leftMargin(), 0, -shellSurface->rightMargin(), 0);
        } else if (shellSurface->anchor() & Qt::LeftEdge) {
            geometry.translate(shellSurface->leftMargin(), 0);
        } else if (shellSurface->anchor() & Qt::RightEdge) {
            geometry.translate(-shellSurface->rightMargin(), 0);
        }

        if ((shellSurface->anchor() & AnchorVertical) == AnchorVertical) {
            geometry.adjust(0, shellSurface->topMargin(), 0, -shellSurface->bottomMargin());
        } else if (shellSurface->anchor() & Qt::TopEdge) {
            geometry.translate(0, shellSurface->topMargin());
        } else if (shellSurface->anchor() & Qt::BottomEdge) {
            geometry.translate(0, -shellSurface->bottomMargin());
        }

        if (geometry.isValid()) {
            client->setFrameGeometry(geometry);
        } else {
            qCWarning(KWIN_CORE) << "Closing a layer shell client due to invalid geometry";
            client->closeWindow();
            continue;
        }

        if (exclusive && shellSurface->exclusiveZone() > 0) {
            adjustWorkArea(shellSurface, workArea);
        }
    }
}

static QList<LayerShellV1Client *> clientsForOutput(AbstractOutput *output)
{
    QList<LayerShellV1Client *> result;
    const QList<AbstractClient *> clients = waylandServer()->clients();
    for (AbstractClient *client : clients) {
        LayerShellV1Client *layerShellClient = qobject_cast<LayerShellV1Client *>(client);
        if (!layerShellClient || layerShellClient->output() != output) {
            continue;
        }
        if (layerShellClient->shellSurface()->isCommitted()) {
            result.append(layerShellClient);
        }
    }
    return result;
}

static void rearrangeOutput(AbstractOutput *output)
{
    const QList<LayerShellV1Client *> clients = clientsForOutput(output);
    if (!clients.isEmpty()) {
        QRect workArea = output->geometry();

        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::OverlayLayer, true);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::TopLayer, true);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::BottomLayer, true);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::BackgroundLayer, true);

        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::OverlayLayer, false);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::TopLayer, false);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::BottomLayer, false);
        rearrangeLayer(clients, &workArea, LayerSurfaceV1Interface::BackgroundLayer, false);
    }
}

void LayerShellV1Integration::rearrange()
{
    m_rearrangeTimer->stop();

    const QVector<AbstractOutput *> outputs = kwinApp()->platform()->outputs();
    for (AbstractOutput *output : outputs) {
        rearrangeOutput(output);
    }

    workspace()->updateClientArea();
}

void LayerShellV1Integration::scheduleRearrange()
{
    m_rearrangeTimer->start();
}

} // namespace KWin
