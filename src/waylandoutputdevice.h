/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "abstract_wayland_output.h"

#include <KWaylandServer/outputdevice_interface.h>

namespace KWin
{

class WaylandOutputDevice : public QObject
{
    Q_OBJECT

public:
    explicit WaylandOutputDevice(AbstractWaylandOutput *output, QObject *parent = nullptr);

private Q_SLOTS:
    void handleGeometryChanged();
    void handleScaleChanged();
    void handleEnabledChanged();
    void handleTransformChanged();
    void handleModeChanged();
    void handleCapabilitiesChanged();
    void handleOverscanChanged();

private:
    AbstractWaylandOutput *m_platformOutput;
    KWaylandServer::OutputDeviceInterface *m_outputDevice;
};

} // namespace KWin
