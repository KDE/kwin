/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandclient.h"

namespace KWaylandServer
{
class LayerSurfaceV1Interface;
}

namespace KWin
{

class AbstractOutput;
class LayerShellV1Integration;

class LayerShellV1Client : public WaylandClient
{
    Q_OBJECT

public:
    explicit LayerShellV1Client(KWaylandServer::LayerSurfaceV1Interface *shellSurface,
                                AbstractOutput *output,
                                LayerShellV1Integration *integration);

    KWaylandServer::LayerSurfaceV1Interface *shellSurface() const;
    AbstractOutput *output() const;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool isPlaceable() const override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool isInitialPositionSet() const override;
    bool takeFocus() override;
    bool wantsInput() const override;
    StrutRect strutRect(StrutArea area) const override;
    bool hasStrut() const override;
    void destroyClient() override;
    void closeWindow() override;

protected:
    Layer belongsToLayer() const override;
    bool acceptsFocus() const override;
    void requestGeometry(const QRect &rect) override;
    void addDamage(const QRegion &region) override;

private:
    void handleSizeChanged();
    void handleUnmapped();
    void handleCommitted();
    void handleAcceptsFocusChanged();
    void handleOutputDestroyed();
    void scheduleRearrange();

    AbstractOutput *m_output;
    LayerShellV1Integration *m_integration;
    KWaylandServer::LayerSurfaceV1Interface *m_shellSurface;
    NET::WindowType m_windowType;
};

} // namespace KWin
