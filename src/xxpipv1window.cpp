/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xxpipv1window.h"
#include "core/pixelgrid.h"
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

    m_spontaneousGravity = Gravity::Center;
    m_configureGravity = Gravity::Center;

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

bool XXPipV1Window::acceptsFocus() const
{
    return false;
}

XdgSurfaceConfigure *XXPipV1Window::sendRoleConfigure()
{
    surface()->setPreferredBufferScale(nextTargetScale());
    surface()->setPreferredBufferTransform(preferredBufferTransform());
    surface()->setPreferredColorDescription(preferredColorDescription());

    const RectF geometry = moveResizeGeometry();
    if (geometry.isEmpty()) {
        const RectF workArea = workspace()->clientArea(PlacementArea, this, moveResizeOutput());
        m_shellSurface->sendConfigureBounds(workArea.size() * 0.25);
    }

    XdgSurfaceConfigure *configureEvent = new XdgSurfaceConfigure();
    configureEvent->bounds = moveResizeGeometry();
    configureEvent->serial = m_shellSurface->sendConfigureSize(geometry.size());
    configureEvent->gravity = m_configureGravity;
    configureEvent->scale = m_nextTargetScale;

    if (!isInteractiveMoveResize()) {
        m_configureGravity = Gravity::Center;
    }

    return configureEvent;
}

void XXPipV1Window::handleRoleCommit()
{
    const RectF oldWindowGeometry = m_windowGeometry;
    m_windowGeometry = snapToPixels(m_shellSurface->xdgSurface()->windowGeometry(), targetScale());

    RectF frameGeometry(pos(), clientSizeToFrameSize(m_windowGeometry.size()));
    if (isInteractiveMove()) {
        frameGeometry = nextInteractiveMoveGeometry(frameGeometry);
    } else {
        if (const XdgSurfaceConfigure *configureEvent = lastAcknowledgedConfigure()) {
            frameGeometry = configureEvent->gravity.apply(frameGeometry, configureEvent->bounds);
            if (const auto anchor = confineInteractiveMove(frameGeometry)) {
                frameGeometry.moveTopLeft(*anchor);
            }
        } else if (oldWindowGeometry != m_windowGeometry) {
            frameGeometry = m_spontaneousGravity.apply(frameGeometry, m_frameGeometry);
            if (const auto anchor = confineInteractiveMove(frameGeometry)) {
                frameGeometry.moveTopLeft(*anchor);
            }
        }

        const RectF safeArea = workspace()->clientArea(PlacementArea, this)
            .shrunkBy(QMarginsF(options->pictureInPictureMargin(),
                                options->pictureInPictureMargin(),
                                options->pictureInPictureMargin(),
                                options->pictureInPictureMargin()));
        const qreal snapDistance = options->pictureInPictureMargin() / 2.0;

        if (m_frameGeometry.right() - safeArea.right() < snapDistance) {
            if (frameGeometry.right() - safeArea.right() > -snapDistance
                || m_frameGeometry.right() - safeArea.right() > -snapDistance) {
                frameGeometry.moveRight(safeArea.right());
            }
        }

        if (m_frameGeometry.left() - safeArea.left() > -snapDistance) {
            if (frameGeometry.left() - safeArea.left() < snapDistance
                || m_frameGeometry.left() - safeArea.left() < snapDistance) {
                frameGeometry.moveLeft(safeArea.left());
            }
        }

        if (m_frameGeometry.bottom() - safeArea.bottom() < snapDistance) {
            if (frameGeometry.bottom() - safeArea.bottom() > -snapDistance
                || m_frameGeometry.bottom() - safeArea.bottom() > -snapDistance) {
                frameGeometry.moveBottom(safeArea.bottom());
            }
        }

        if (m_frameGeometry.top() - safeArea.top() > -snapDistance) {
            if (frameGeometry.top() - safeArea.top() < snapDistance
                || m_frameGeometry.top() - safeArea.top() < snapDistance) {
                frameGeometry.moveTop(safeArea.top());
            }
        }
    }

    if (!m_configureTimer->isActive() && m_configureEvents.isEmpty()) {
        setMoveResizeGeometry(frameGeometry);
    }

    updateGeometry(frameGeometry);

    if (const auto configureEvent = lastAcknowledgedConfigure()) {
        if (!m_configureEvents.isEmpty()) {
            m_spontaneousGravity = configureEvent->gravity;
        } else if (!isInteractiveResize()) {
            m_spontaneousGravity = Gravity::Center;
        }
    }
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
    updateInteractiveMoveResizeCursor();
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

bool XXPipV1Window::doStartInteractiveMoveResize()
{
    if (interactiveMoveResizeGravity() != Gravity::Center) {
        m_configureGravity = interactiveMoveResizeGravity();
        scheduleConfigure();
    }

    return true;
}

void XXPipV1Window::doFinishInteractiveMoveResize()
{
    scheduleConfigure();
}

} // namespace KWin
