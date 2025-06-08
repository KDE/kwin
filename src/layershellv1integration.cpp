/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layershellv1integration.h"
#include "core/output.h"
#include "layershellv1window.h"
#include "wayland/display.h"
#include "wayland/layershell_v1.h"
#include "wayland/output.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

static const Qt::Edges AnchorHorizontal = Qt::LeftEdge | Qt::RightEdge;
static const Qt::Edges AnchorVertical = Qt::TopEdge | Qt::BottomEdge;

LayerShellV1Integration::LayerShellV1Integration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    LayerShellV1Interface *shell = new LayerShellV1Interface(waylandServer()->display(), this);
    connect(shell, &LayerShellV1Interface::surfaceCreated,
            this, &LayerShellV1Integration::createWindow);

    connect(workspace(), &Workspace::aboutToRearrange, this, &LayerShellV1Integration::rearrange);
}

void LayerShellV1Integration::createWindow(LayerSurfaceV1Interface *shellSurface)
{
    Output *output;
    if (OutputInterface *preferredOutput = shellSurface->output()) {
        if (preferredOutput->isRemoved()) {
            shellSurface->sendClosed();
            return;
        }
        output = preferredOutput->handle();
    } else {
        output = workspace()->activeOutput();
    }

    Q_EMIT windowCreated(new LayerShellV1Window(shellSurface, output, this));
}

void LayerShellV1Integration::recreateWindow(LayerSurfaceV1Interface *shellSurface)
{
    destroyWindow(shellSurface);
    createWindow(shellSurface);
}

void LayerShellV1Integration::destroyWindow(LayerSurfaceV1Interface *shellSurface)
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

static int weightForWindow(const LayerShellV1Window *window)
{
    return (window->shellSurface()->anchor() & AnchorHorizontal) == AnchorHorizontal ? 1 : 0;
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
    std::stable_sort(result.begin(), result.end(), [](LayerShellV1Window *a, LayerShellV1Window *b) {
        const bool exclusiveA = a->shellSurface()->exclusiveZone() > 0;
        const bool exclusiveB = b->shellSurface()->exclusiveZone() > 0;

        if (exclusiveA && !exclusiveB) {
            return true;
        } else if (!exclusiveA && exclusiveB) {
            return false;
        }

        if (a->layer() < b->layer()) {
            return false;
        } else if (a->layer() > b->layer()) {
            return true;
        }

        return weightForWindow(a) > weightForWindow(b);
    });
    return result;
}

static void rearrangeOutput(Output *output)
{
    const QList<LayerShellV1Window *> windows = windowsForOutput(output);
    QRect workArea = output->geometry();

    for (LayerShellV1Window *window : windows) {
        LayerSurfaceV1Interface *shellSurface = window->shellSurface();

        QRect bounds;
        if (shellSurface->exclusiveZone() == -1) {
            bounds = window->desiredOutput()->geometry();
        } else {
            bounds = workArea;
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

        window->updateLayer();

        if (geometry.isValid()) {
            window->place(geometry);
        } else {
            qCWarning(KWIN_CORE) << "Closing a layer shell window due to invalid geometry";
            window->closeWindow();
            continue;
        }

        if (shellSurface->exclusiveZone() > 0) {
            adjustWorkArea(shellSurface, &workArea);
        }
    }
}

void LayerShellV1Integration::rearrange()
{
    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        rearrangeOutput(output);
    }
}

} // namespace KWin

#include "moc_layershellv1integration.cpp"
