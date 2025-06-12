/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xxpipv1window.h"
#include "input.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/tablet_v2.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

XXPipV1Window::XXPipV1Window(XXPipV1Interface *shellSurface)
    : XdgSurfaceWindow(shellSurface->xdgSurface())
    , m_shellSurface(shellSurface)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());
    setOnAllDesktops(true);
    setOnAllActivities(true);

    connect(shellSurface, &XXPipV1Interface::initializeRequested,
            this, &XXPipV1Window::initialize);
    connect(shellSurface, &XXPipV1Interface::aboutToBeDestroyed,
            this, &XXPipV1Window::destroyWindow);
    connect(shellSurface, &XXPipV1Interface::moveRequested,
            this, &XXPipV1Window::handleMoveRequested);
    connect(shellSurface, &XXPipV1Interface::resizeRequested,
            this, &XXPipV1Window::handleResizeRequested);
    connect(shellSurface, &XXPipV1Interface::applicationIdChanged,
            this, &XXPipV1Window::handleApplicationIdChanged);
}

void XXPipV1Window::initialize()
{
    scheduleConfigure();
}

bool XXPipV1Window::isPictureInPicture() const
{
    return true;
}

bool XXPipV1Window::isResizable() const
{
    return true;
}

bool XXPipV1Window::isMovable() const
{
    return true;
}

bool XXPipV1Window::isMovableAcrossScreens() const
{
    return true;
}

bool XXPipV1Window::isCloseable() const
{
    return true;
}

void XXPipV1Window::closeWindow()
{
    m_shellSurface->sendClosed();
}

bool XXPipV1Window::wantsInput() const
{
    return false;
}

bool XXPipV1Window::takeFocus()
{
    return false;
}

bool XXPipV1Window::acceptsFocus() const
{
    return false;
}

XdgSurfaceConfigure *XXPipV1Window::sendRoleConfigure() const
{
    surface()->setPreferredBufferScale(nextTargetScale());
    surface()->setPreferredBufferTransform(preferredBufferTransform());
    surface()->setPreferredColorDescription(preferredColorDescription());

    const QRectF geometry = moveResizeGeometry();
    if (geometry.isEmpty()) {
        const QRectF workArea = workspace()->clientArea(PlacementArea, this, moveResizeOutput());
        m_shellSurface->sendConfigureBounds(workArea.size() * 0.25);
    }

    XdgSurfaceConfigure *configureEvent = new XdgSurfaceConfigure();
    configureEvent->bounds = moveResizeGeometry();
    configureEvent->serial = m_shellSurface->sendConfigureSize(geometry.size());

    return configureEvent;
}

void XXPipV1Window::handleRoleDestroyed()
{
    m_shellSurface->disconnect(this);

    XdgSurfaceWindow::handleRoleDestroyed();
}

void XXPipV1Window::handleApplicationIdChanged()
{
    setResourceClass(resourceName(), m_shellSurface->applicationId());
    setDesktopFileName(m_shellSurface->applicationId());
}

void XXPipV1Window::handleMoveRequested(SeatInterface *seat, quint32 serial)
{
    if (const auto anchor = input()->implicitGrabPositionBySerial(seat, serial)) {
        performMousePressCommand(Options::MouseMove, *anchor);
    }
}

void XXPipV1Window::handleResizeRequested(SeatInterface *seat, Gravity gravity, quint32 serial)
{
    const auto anchor = input()->implicitGrabPositionBySerial(seat, serial);
    if (!anchor) {
        return;
    }
    if (isInteractiveMoveResize()) {
        finishInteractiveMoveResize(false);
    }
    setInteractiveMoveResizePointerButtonDown(true);
    setInteractiveMoveResizeAnchor(*anchor);
    setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers());
    setInteractiveMoveOffset(QPointF((anchor->x() - x()) / width(), (anchor->y() - y()) / height()));
    setUnrestrictedInteractiveMoveResize(false);
    setInteractiveMoveResizeGravity(gravity);
    if (!startInteractiveMoveResize()) {
        setInteractiveMoveResizePointerButtonDown(false);
    }
    updateCursor();
}

void XXPipV1Window::doSetNextTargetScale()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

void XXPipV1Window::doSetPreferredBufferTransform()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

void XXPipV1Window::doSetPreferredColorDescription()
{
    if (isDeleted()) {
        return;
    }
    if (m_shellSurface->isConfigured()) {
        scheduleConfigure();
    }
}

} // namespace KWin
