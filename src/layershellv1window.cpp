/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layershellv1window.h"
#include "core/output.h"
#include "deleted.h"
#include "layershellv1integration.h"
#include "wayland/layershell_v1_interface.h"
#include "wayland/output_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "workspace.h"

using namespace KWaylandServer;

namespace KWin
{

static NET::WindowType scopeToType(const QString &scope)
{
    static const QHash<QString, NET::WindowType> scopeToType{
        {QStringLiteral("desktop"), NET::Desktop},
        {QStringLiteral("dock"), NET::Dock},
        {QStringLiteral("crititical-notification"), NET::CriticalNotification},
        {QStringLiteral("notification"), NET::Notification},
        {QStringLiteral("tooltip"), NET::Tooltip},
        {QStringLiteral("on-screen-display"), NET::OnScreenDisplay},
        {QStringLiteral("dialog"), NET::Dialog},
        {QStringLiteral("splash"), NET::Splash},
        {QStringLiteral("utility"), NET::Utility},
    };
    return scopeToType.value(scope.toLower(), NET::Normal);
}

LayerShellV1Window::LayerShellV1Window(LayerSurfaceV1Interface *shellSurface,
                                       Output *output,
                                       LayerShellV1Integration *integration)
    : WaylandWindow(shellSurface->surface())
    , m_desiredOutput(output)
    , m_integration(integration)
    , m_shellSurface(shellSurface)
    , m_windowType(scopeToType(shellSurface->scope()))
{
    setSkipSwitcher(!isDesktop());
    setSkipPager(true);
    setSkipTaskbar(true);

    connect(shellSurface, &LayerSurfaceV1Interface::aboutToBeDestroyed,
            this, &LayerShellV1Window::destroyWindow);
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed,
            this, &LayerShellV1Window::destroyWindow);

    connect(output, &Output::geometryChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(output, &Output::enabledChanged,
            this, &LayerShellV1Window::handleOutputEnabledChanged);
    connect(output, &Output::destroyed,
            this, &LayerShellV1Window::handleOutputDestroyed);

    connect(shellSurface->surface(), &SurfaceInterface::sizeChanged,
            this, &LayerShellV1Window::handleSizeChanged);
    connect(shellSurface->surface(), &SurfaceInterface::unmapped,
            this, &LayerShellV1Window::handleUnmapped);
    connect(shellSurface->surface(), &SurfaceInterface::committed,
            this, &LayerShellV1Window::handleCommitted);

    connect(shellSurface, &LayerSurfaceV1Interface::desiredSizeChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::layerChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::marginsChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::anchorChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::exclusiveZoneChanged,
            this, &LayerShellV1Window::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::acceptsFocusChanged,
            this, &LayerShellV1Window::handleAcceptsFocusChanged);
}

LayerSurfaceV1Interface *LayerShellV1Window::shellSurface() const
{
    return m_shellSurface;
}

Output *LayerShellV1Window::desiredOutput() const
{
    return m_desiredOutput;
}

void LayerShellV1Window::scheduleRearrange()
{
    m_integration->scheduleRearrange();
}

NET::WindowType LayerShellV1Window::windowType(bool, int) const
{
    return m_windowType;
}

bool LayerShellV1Window::isPlaceable() const
{
    return false;
}

bool LayerShellV1Window::isCloseable() const
{
    return true;
}

bool LayerShellV1Window::isMovable() const
{
    return false;
}

bool LayerShellV1Window::isMovableAcrossScreens() const
{
    return false;
}

bool LayerShellV1Window::isResizable() const
{
    return false;
}

bool LayerShellV1Window::takeFocus()
{
    setActive(true);
    return true;
}

bool LayerShellV1Window::wantsInput() const
{
    return acceptsFocus() && readyForPainting();
}

StrutRect LayerShellV1Window::strutRect(StrutArea area) const
{
    switch (area) {
    case StrutAreaLeft:
        if (m_shellSurface->exclusiveEdge() == Qt::LeftEdge) {
            return StrutRect(x(), y(), m_shellSurface->exclusiveZone(), height(), StrutAreaLeft);
        }
        return StrutRect();
    case StrutAreaRight:
        if (m_shellSurface->exclusiveEdge() == Qt::RightEdge) {
            return StrutRect(x() + width() - m_shellSurface->exclusiveZone(), y(),
                             m_shellSurface->exclusiveZone(), height(), StrutAreaRight);
        }
        return StrutRect();
    case StrutAreaTop:
        if (m_shellSurface->exclusiveEdge() == Qt::TopEdge) {
            return StrutRect(x(), y(), width(), m_shellSurface->exclusiveZone(), StrutAreaTop);
        }
        return StrutRect();
    case StrutAreaBottom:
        if (m_shellSurface->exclusiveEdge() == Qt::BottomEdge) {
            return StrutRect(x(), y() + height() - m_shellSurface->exclusiveZone(),
                             width(), m_shellSurface->exclusiveZone(), StrutAreaBottom);
        }
        return StrutRect();
    default:
        return StrutRect();
    }
}

bool LayerShellV1Window::hasStrut() const
{
    return m_shellSurface->exclusiveZone() > 0;
}

void LayerShellV1Window::destroyWindow()
{
    markAsZombie();
    cleanTabBox();
    Deleted *deleted = Deleted::create(this);
    Q_EMIT windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    cleanGrouping();
    waylandServer()->removeWindow(this);
    deleted->unrefWindow();
    scheduleRearrange();
    delete this;
}

void LayerShellV1Window::closeWindow()
{
    m_shellSurface->sendClosed();
}

Layer LayerShellV1Window::belongsToLayer() const
{
    if (!isNormalWindow()) {
        return WaylandWindow::belongsToLayer();
    }
    switch (m_shellSurface->layer()) {
    case LayerSurfaceV1Interface::BackgroundLayer:
        return DesktopLayer;
    case LayerSurfaceV1Interface::BottomLayer:
        return BelowLayer;
    case LayerSurfaceV1Interface::TopLayer:
        return AboveLayer;
    case LayerSurfaceV1Interface::OverlayLayer:
        return UnmanagedLayer;
    default:
        Q_UNREACHABLE();
    }
}

bool LayerShellV1Window::acceptsFocus() const
{
    return !isDeleted() && m_shellSurface->acceptsFocus();
}

void LayerShellV1Window::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    if (areGeometryUpdatesBlocked()) {
        setPendingMoveResizeMode(mode);
        return;
    }

    const QSizeF requestedClientSize = frameSizeToClientSize(rect.size());
    if (requestedClientSize != clientSize()) {
        m_shellSurface->sendConfigure(rect.size().toSize());
    } else {
        updateGeometry(rect);
        return;
    }

    // The surface position is updated synchronously.
    QRectF updateRect = m_frameGeometry;
    updateRect.moveTopLeft(rect.topLeft());
    updateGeometry(updateRect);
}

void LayerShellV1Window::handleSizeChanged()
{
    updateGeometry(QRectF(pos(), clientSizeToFrameSize(surface()->size())));
    scheduleRearrange();
}

void LayerShellV1Window::handleUnmapped()
{
    m_integration->recreateWindow(shellSurface());
}

void LayerShellV1Window::handleCommitted()
{
    if (surface()->buffer()) {
        updateDepth();
        setReadyForPainting();
    }
}

void LayerShellV1Window::handleAcceptsFocusChanged()
{
    switch (m_shellSurface->layer()) {
    case LayerSurfaceV1Interface::TopLayer:
    case LayerSurfaceV1Interface::OverlayLayer:
        if (wantsInput()) {
            workspace()->activateWindow(this);
        }
        break;
    case LayerSurfaceV1Interface::BackgroundLayer:
    case LayerSurfaceV1Interface::BottomLayer:
        break;
    }
}

void LayerShellV1Window::handleOutputEnabledChanged()
{
    if (!m_desiredOutput->isEnabled()) {
        closeWindow();
        destroyWindow();
    }
}

void LayerShellV1Window::handleOutputDestroyed()
{
    closeWindow();
    destroyWindow();
}

void LayerShellV1Window::setVirtualKeyboardGeometry(const QRectF &geo)
{
    if (m_virtualKeyboardGeometry == geo) {
        return;
    }

    m_virtualKeyboardGeometry = geo;
    scheduleRearrange();
}

} // namespace KWin
