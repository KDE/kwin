/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layershellv1integration.h"
#include "core/output.h"
#include "layershellv1window.h"
#include "wayland/display.h"
#include "wayland/layershell_v1_interface.h"
#include "wayland/output_interface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QTimer>

namespace KWin
{

static const Qt::Edges AnchorHorizontal = Qt::LeftEdge | Qt::RightEdge;
static const Qt::Edges AnchorVertical = Qt::TopEdge | Qt::BottomEdge;

LayerShellV1Integration::LayerShellV1Integration()
    : m_shell(std::make_unique<KWaylandServer::LayerShellV1Interface>(waylandServer()->display()))
{
    connect(m_shell.get(), &KWaylandServer::LayerShellV1Interface::surfaceCreated,
            this, &LayerShellV1Integration::createWindow);

    m_rearrangeTimer = new QTimer(this);
    m_rearrangeTimer->setSingleShot(true);
    connect(m_rearrangeTimer, &QTimer::timeout, this, &LayerShellV1Integration::rearrange);
}

LayerShellV1Integration::~LayerShellV1Integration() = default;

void LayerShellV1Integration::createWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface)
{
    Output *output = shellSurface->output() ? shellSurface->output()->handle() : workspace()->activeOutput();
    if (!output) {
        qCWarning(KWIN_CORE) << "Could not find any suitable output for a layer surface";
        shellSurface->sendClosed();
        return;
    }

    Q_EMIT windowCreated(new LayerShellV1Window(shellSurface, output, this));
}

void LayerShellV1Integration::recreateWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface)
{
    destroyWindow(shellSurface);
    createWindow(shellSurface);
}

void LayerShellV1Integration::destroyWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface)
{
    const QList<Window *> windows = waylandServer()->windows();
    for (Window *window : windows) {
        LayerShellV1Window *layerShellWindow = qobject_cast<LayerShellV1Window *>(window);
        if (layerShellWindow && layerShellWindow->shellSurface() == shellSurface) {
            layerShellWindow->destroyWindow();
            break;
        }
    }
}

static void adjustWorkArea(const KWaylandServer::LayerSurfaceV1Interface *shellSurface, QRect *workArea)
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

static void rearrangeLayer(const QList<LayerShellV1Window *> &windows, QRect *workArea,
                           KWaylandServer::LayerSurfaceV1Interface::Layer layer, bool exclusive)
{
    for (LayerShellV1Window *window : windows) {
        KWaylandServer::LayerSurfaceV1Interface *shellSurface = window->shellSurface();

        if (shellSurface->layer() != layer) {
            continue;
        }
        if (exclusive != (shellSurface->exclusiveZone() > 0)) {
            continue;
        }

        QRect bounds;
        if (shellSurface->exclusiveZone() == -1) {
            bounds = window->desiredOutput()->geometry();
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

        // Move the window's bottom if its virtual keyboard is overlapping it
        if (shellSurface->exclusiveZone() >= 0 && !window->virtualKeyboardGeometry().isEmpty() && geometry.bottom() > window->virtualKeyboardGeometry().top()) {
            geometry.setBottom(window->virtualKeyboardGeometry().top());
        }

        if (geometry.isValid()) {
            window->moveResize(geometry);
        } else {
            qCWarning(KWIN_CORE) << "Closing a layer shell window due to invalid geometry";
            window->closeWindow();
            continue;
        }

        if (exclusive && shellSurface->exclusiveZone() > 0) {
            adjustWorkArea(shellSurface, workArea);
        }
    }
}

static QList<LayerShellV1Window *> windowsForOutput(Output *output)
{
    QList<LayerShellV1Window *> result;
    const QList<Window *> windows = waylandServer()->windows();
    for (Window *window : windows) {
        LayerShellV1Window *layerShellWindow = qobject_cast<LayerShellV1Window *>(window);
        if (!layerShellWindow || layerShellWindow->desiredOutput() != output) {
            continue;
        }
        if (layerShellWindow->shellSurface()->isCommitted()) {
            result.append(layerShellWindow);
        }
    }
    return result;
}

static void rearrangeOutput(Output *output)
{
    const QList<LayerShellV1Window *> windows = windowsForOutput(output);
    if (!windows.isEmpty()) {
        QRect workArea = output->geometry();

        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::OverlayLayer, true);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::TopLayer, true);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::BottomLayer, true);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::BackgroundLayer, true);

        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::OverlayLayer, false);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::TopLayer, false);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::BottomLayer, false);
        rearrangeLayer(windows, &workArea, KWaylandServer::LayerSurfaceV1Interface::BackgroundLayer, false);
    }
}

void LayerShellV1Integration::rearrange()
{
    m_rearrangeTimer->stop();

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        rearrangeOutput(output);
    }

    if (workspace()) {
        workspace()->updateClientArea();
    }
}

void LayerShellV1Integration::scheduleRearrange()
{
    m_rearrangeTimer->start();
}

} // namespace KWin
