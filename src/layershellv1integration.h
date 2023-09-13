/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "waylandshellintegration.h"

namespace KWin
{

class LayerSurfaceV1Interface;

class LayerShellV1Integration : public WaylandShellIntegration
{
    Q_OBJECT

public:
    explicit LayerShellV1Integration(QObject *parent = nullptr);

    void rearrange();
    void scheduleRearrange();

    void createWindow(LayerSurfaceV1Interface *shellSurface);
    void recreateWindow(LayerSurfaceV1Interface *shellSurface);
    void destroyWindow(LayerSurfaceV1Interface *shellSurface);

private:
    QTimer *m_rearrangeTimer;
};

} // namespace KWin
