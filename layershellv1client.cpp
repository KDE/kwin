/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "layershellv1client.h"
#include "abstract_output.h"
#include "layershellv1integration.h"
#include "deleted.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWaylandServer/layershell_v1_interface.h>
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/surface_interface.h>

using namespace KWaylandServer;

namespace KWin
{

static NET::WindowType scopeToType(const QString &scope)
{
    static const QHash<QString, NET::WindowType> scopeToType {
        { QStringLiteral("desktop"), NET::Desktop },
        { QStringLiteral("dock"), NET::Dock },
        { QStringLiteral("crititical-notification"), NET::CriticalNotification },
        { QStringLiteral("notification"), NET::Notification },
        { QStringLiteral("tooltip"), NET::Tooltip },
        { QStringLiteral("on-screen-display"), NET::OnScreenDisplay },
        { QStringLiteral("dialog"), NET::Dialog },
        { QStringLiteral("splash"), NET::Splash },
        { QStringLiteral("utility"), NET::Utility },
    };
    return scopeToType.value(scope.toLower(), NET::Normal);
}

LayerShellV1Client::LayerShellV1Client(LayerSurfaceV1Interface *shellSurface,
                                       AbstractOutput *output,
                                       LayerShellV1Integration *integration)
    : WaylandClient(shellSurface->surface())
    , m_output(output)
    , m_integration(integration)
    , m_shellSurface(shellSurface)
    , m_windowType(scopeToType(shellSurface->scope()))
{
    setSkipSwitcher(!isDesktop());
    setSkipPager(true);
    setSkipTaskbar(true);
    setSizeSyncMode(SyncMode::Async);
    setPositionSyncMode(SyncMode::Sync);

    connect(shellSurface, &LayerSurfaceV1Interface::aboutToBeDestroyed,
            this, &LayerShellV1Client::destroyClient);
    connect(shellSurface->surface(), &SurfaceInterface::aboutToBeDestroyed,
            this, &LayerShellV1Client::destroyClient);

    connect(output, &AbstractOutput::geometryChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(output, &AbstractOutput::destroyed,
            this, &LayerShellV1Client::handleOutputDestroyed);

    connect(shellSurface->surface(), &SurfaceInterface::sizeChanged,
            this, &LayerShellV1Client::handleSizeChanged);
    connect(shellSurface->surface(), &SurfaceInterface::unmapped,
            this, &LayerShellV1Client::handleUnmapped);
    connect(shellSurface->surface(), &SurfaceInterface::committed,
            this, &LayerShellV1Client::handleCommitted);

    connect(shellSurface, &LayerSurfaceV1Interface::desiredSizeChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::layerChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::marginsChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::anchorChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::exclusiveZoneChanged,
            this, &LayerShellV1Client::scheduleRearrange);
    connect(shellSurface, &LayerSurfaceV1Interface::acceptsFocusChanged,
            this, &LayerShellV1Client::handleAcceptsFocusChanged);
}

LayerSurfaceV1Interface *LayerShellV1Client::shellSurface() const
{
    return m_shellSurface;
}

AbstractOutput *LayerShellV1Client::output() const
{
    return m_output;
}

void LayerShellV1Client::scheduleRearrange()
{
    m_integration->scheduleRearrange();
}

NET::WindowType LayerShellV1Client::windowType(bool, int) const
{
    return m_windowType;
}

bool LayerShellV1Client::isPlaceable() const
{
    return false;
}

bool LayerShellV1Client::isCloseable() const
{
    return true;
}

bool LayerShellV1Client::isMovable() const
{
    return false;
}

bool LayerShellV1Client::isMovableAcrossScreens() const
{
    return false;
}

bool LayerShellV1Client::isResizable() const
{
    return false;
}

bool LayerShellV1Client::isInitialPositionSet() const
{
    return true;
}

bool LayerShellV1Client::takeFocus()
{
    setActive(true);
    return true;
}

bool LayerShellV1Client::wantsInput() const
{
    return acceptsFocus() && readyForPainting();
}

StrutRect LayerShellV1Client::strutRect(StrutArea area) const
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

bool LayerShellV1Client::hasStrut() const
{
    return m_shellSurface->exclusiveZone() > 0;
}

void LayerShellV1Client::destroyClient()
{
    markAsZombie();
    cleanTabBox();
    Deleted *deleted = Deleted::create(this);
    emit windowClosed(this, deleted);
    StackingUpdatesBlocker blocker(workspace());
    cleanGrouping();
    waylandServer()->removeClient(this);
    deleted->unrefWindow();
    scheduleRearrange();
    delete this;
}

void LayerShellV1Client::closeWindow()
{
    m_shellSurface->sendClosed();
}

Layer LayerShellV1Client::belongsToLayer() const
{
    if (!isNormalWindow()) {
        return WaylandClient::belongsToLayer();
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

bool LayerShellV1Client::acceptsFocus() const
{
    return m_shellSurface->acceptsFocus();
}

void LayerShellV1Client::addDamage(const QRegion &region)
{
    addRepaint(region);
    WaylandClient::addDamage(region);
}

void LayerShellV1Client::requestGeometry(const QRect &rect)
{
    WaylandClient::requestGeometry(rect);
    m_shellSurface->sendConfigure(rect.size());
}

void LayerShellV1Client::handleSizeChanged()
{
    updateGeometry(QRect(pos(), clientSizeToFrameSize(surface()->size())));
    scheduleRearrange();
}

void LayerShellV1Client::handleUnmapped()
{
    m_integration->recreateClient(shellSurface());
}

void LayerShellV1Client::handleCommitted()
{
    if (surface()->buffer()) {
        updateDepth();
        setReadyForPainting();
    }
}

void LayerShellV1Client::handleAcceptsFocusChanged()
{
    switch (m_shellSurface->layer()) {
    case LayerSurfaceV1Interface::TopLayer:
    case LayerSurfaceV1Interface::OverlayLayer:
        if (wantsInput()) {
            workspace()->activateClient(this);
        }
        break;
    case LayerSurfaceV1Interface::BackgroundLayer:
    case LayerSurfaceV1Interface::BottomLayer:
        break;
    }
}

void LayerShellV1Client::handleOutputDestroyed()
{
    closeWindow();
    destroyClient();
}

} // namespace KWin
