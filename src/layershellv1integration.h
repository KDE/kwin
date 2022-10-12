/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWaylandServer
{
class LayerSurfaceV1Interface;
class LayerShellV1Interface;
}

namespace KWin
{

class LayerShellV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit LayerShellV1Integration();
    ~LayerShellV1Integration();

    void rearrange();
    void scheduleRearrange();

    void createWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface);
    void recreateWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface);
    void destroyWindow(KWaylandServer::LayerSurfaceV1Interface *shellSurface);

private:
    std::unique_ptr<KWaylandServer::LayerShellV1Interface> m_shell;
    QTimer *m_rearrangeTimer;
};

} // namespace KWin
