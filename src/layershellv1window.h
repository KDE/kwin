/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandwindow.h"

namespace KWaylandServer
{
class LayerSurfaceV1Interface;
}

namespace KWin
{

class Output;
class LayerShellV1Integration;

class LayerShellV1Window : public WaylandWindow
{
    Q_OBJECT

public:
    explicit LayerShellV1Window(KWaylandServer::LayerSurfaceV1Interface *shellSurface,
                                Output *output,
                                LayerShellV1Integration *integration);

    KWaylandServer::LayerSurfaceV1Interface *shellSurface() const;
    Output *desiredOutput() const;

    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool isPlaceable() const override;
    bool isCloseable() const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool takeFocus() override;
    bool wantsInput() const override;
    StrutRect strutRect(StrutArea area) const override;
    bool hasStrut() const override;
    void destroyWindow() override;
    void closeWindow() override;
    void setVirtualKeyboardGeometry(const QRectF &geo) override;

protected:
    Layer belongsToLayer() const override;
    bool acceptsFocus() const override;
    void moveResizeInternal(const QRectF &rect, MoveResizeMode mode) override;

private:
    void handleSizeChanged();
    void handleUnmapped();
    void handleCommitted();
    void handleAcceptsFocusChanged();
    void handleOutputEnabledChanged();
    void handleOutputDestroyed();
    void scheduleRearrange();

    Output *m_desiredOutput;
    LayerShellV1Integration *m_integration;
    KWaylandServer::LayerSurfaceV1Interface *m_shellSurface;
    NET::WindowType m_windowType;
};

} // namespace KWin
